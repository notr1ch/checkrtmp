#include "rtmp.h"
#include "amf.h"
#include "log.h"

#include <time.h>
#include <math.h>

#define SAVC(x)    static const AVal av_##x = AVC(#x)
#define STR2AVAL(av,str) av.av_val = str; av.av_len = (int)strlen(av.av_val)

SAVC(app);
SAVC(connect);
SAVC(flashVer);
SAVC(swfUrl);
SAVC(pageUrl);
SAVC(tcUrl);
SAVC(fpad);
SAVC(capabilities);
SAVC(audioCodecs);
SAVC(videoCodecs);
SAVC(videoFunction);
SAVC(objectEncoding);
SAVC(_result);
SAVC(FCSubscribe);
SAVC(onFCSubscribe);
SAVC(createStream);
SAVC(deleteStream);
SAVC(getStreamLength);
SAVC(play);
SAVC(fmsVer);
SAVC(mode);
SAVC(level);
SAVC(code);
SAVC(description);
SAVC(secureToken);

SAVC(send);

SAVC(onMetaData);
SAVC(duration);
SAVC(width);
SAVC(height);
SAVC(videocodecid);
SAVC(videodatarate);
SAVC(framerate);
SAVC(audiocodecid);
SAVC(audiodatarate);
SAVC(audiosamplerate);
SAVC(audiosamplesize);
SAVC(audiochannels);
SAVC(stereo);
SAVC(encoder);
SAVC(fileSize);

SAVC(onStatus);
SAVC(status);
SAVC(details);
SAVC(clientid);

SAVC(avc1);
SAVC(mp4a);

uint64_t GetTickCount64 ()
{
	struct timeval  tv;
	gettimeofday(&tv, NULL);

	uint64_t time_in_mill = 
		(tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;

	return time_in_mill;
}

static const AVal av_NetStream_Play_Start = AVC("NetStream.Play.Start");
static const AVal av_Started_playing = AVC("Started playing");
static const AVal av_NetStream_Play_Stop = AVC("NetStream.Play.Stop");
static const AVal av_Stopped_playing = AVC("Stopped playing");

static const AVal av_OBSVersion = AVC("TwitchStatus/1.0");
static const AVal av_setDataFrame = AVC("@setDataFrame");

void SendRTMPMetadata(RTMP *rtmp)
{
	char metadata[2048] = {0};
	char *enc = metadata + RTMP_MAX_HEADER_SIZE, *pend = metadata + sizeof(metadata) - RTMP_MAX_HEADER_SIZE;

	enc = AMF_EncodeString(enc, pend, &av_setDataFrame);
	enc = AMF_EncodeString(enc, pend, &av_onMetaData);

	*enc++ = AMF_OBJECT;

	enc = AMF_EncodeNamedNumber(enc, pend, &av_duration, 0.0);
	enc = AMF_EncodeNamedNumber(enc, pend, &av_fileSize, 0.0);
	enc = AMF_EncodeNamedNumber(enc, pend, &av_width, 16);
	enc = AMF_EncodeNamedNumber(enc, pend, &av_height, 16);
	enc = AMF_EncodeNamedString(enc, pend, &av_videocodecid, &av_avc1);//7.0);//
	enc = AMF_EncodeNamedNumber(enc, pend, &av_videodatarate, 10000);
	enc = AMF_EncodeNamedNumber(enc, pend, &av_framerate, 30);
	enc = AMF_EncodeNamedString(enc, pend, &av_audiocodecid, &av_mp4a);//audioCodecID);//
	enc = AMF_EncodeNamedNumber(enc, pend, &av_audiodatarate, 128); //ex. 128kb\s
	enc = AMF_EncodeNamedNumber(enc, pend, &av_audiosamplerate, 44100);
	enc = AMF_EncodeNamedNumber(enc, pend, &av_audiosamplesize, 16.0);
	enc = AMF_EncodeNamedNumber(enc, pend, &av_audiochannels, 2);
	enc = AMF_EncodeNamedBoolean(enc, pend, &av_stereo, 1);

	enc = AMF_EncodeNamedString(enc, pend, &av_encoder, &av_OBSVersion);
	*enc++ = 0;
	*enc++ = 0;
	*enc++ = AMF_OBJECT_END;

	RTMPPacket packet = { 0 };

	packet.m_nChannel = 0x03;     // control channel (invoke)
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_packetType = RTMP_PACKET_TYPE_INFO;
	packet.m_nTimeStamp = 0;
	packet.m_nInfoField2 = rtmp->m_stream_id;
	packet.m_hasAbsTimestamp = TRUE;
	packet.m_body = metadata + RTMP_MAX_HEADER_SIZE;
	packet.m_nBodySize = enc - metadata + RTMP_MAX_HEADER_SIZE;

	RTMP_SendPacket(rtmp, &packet, FALSE);
}

int main (int argc, char *argv[])
{
	int i;
	int tcpBufferSize;
	int *indexOrder = NULL;
	RTMP *rtmp;
	int failed = 0;
	char key[128];

	strcpy (key, argv[2]);
	strcat (key, "?bandwidthtest");

	rtmp = RTMP_Alloc();

	rtmp->m_inChunkSize = RTMP_DEFAULT_CHUNKSIZE;
	rtmp->m_outChunkSize = RTMP_DEFAULT_CHUNKSIZE;
	rtmp->m_bSendChunkSizeInfo = 1;
	rtmp->m_nBufferMS = 30000;
	rtmp->m_nClientBW = 2500000;
	rtmp->m_nClientBW2 = 2;
	rtmp->m_nServerBW = 2500000;
	rtmp->m_fAudioCodecs = 3191.0;
	rtmp->m_fVideoCodecs = 252.0;
	rtmp->Link.timeout = 30;
	rtmp->Link.swfAge = 30;

	rtmp->Link.flashVer.av_val = "FMLE/3.0 (compatible; FMSc/1.0)";
	rtmp->Link.flashVer.av_len = (int)strlen(rtmp->Link.flashVer.av_val);

	rtmp->m_outChunkSize = 4096;
	rtmp->m_bSendChunkSizeInfo = TRUE;

	RTMP_LogSetLevel (RTMP_LOGERROR);
	RTMP_LogSetOutput (stdout);

	RTMP_SetupURL2(rtmp, argv[1], key);

	RTMP_EnableWrite(rtmp);
	rtmp->m_bUseNagle = TRUE;

	if (!RTMP_Connect(rtmp, NULL))
	{
		failed = 1;
		goto abortserver;
	}

	printf ("Connected to server %s\n", argv[1]);

	if (!RTMP_ConnectStream(rtmp, 0))
	{
		failed = 1;
		goto abortserver;
	}

	printf ("Connected to stream %s\n", argv[2]);

	SendRTMPMetadata(rtmp);

	printf ("Metadata sent...\n");

	char junk[4096] = { 0xde };
	RTMPPacket packet = { 0 };

	packet.m_nChannel = 0x05; // source channel
	packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;
	packet.m_body = junk + RTMP_MAX_HEADER_SIZE;
	packet.m_nBodySize = sizeof(junk) - RTMP_MAX_HEADER_SIZE;

	uint64_t realStart = GetTickCount64();
	uint64_t startTime = 0, lastUpdateTime = 0;
	uint64_t bytesSent = 0, startBytes = 0;
	uint64_t sendTime = 0, sendCount = 0;

#define START_MEASUREMENT_WAIT 0
#define TEST_DURATION 5000

	float speed = 0;
	int wasCapped = 0;

	for (;;)
	{
		uint64_t nowTime = GetTickCount64();

		if (!RTMP_SendPacket(rtmp, &packet, FALSE))
		{
			failed = 1;
			break;
		}

		uint64_t diff = GetTickCount64() - nowTime;

		sendTime += diff;
		sendCount++;

		bytesSent += packet.m_nBodySize;

		if (nowTime - realStart > START_MEASUREMENT_WAIT && !startTime)
		{
			startTime = nowTime;
			startBytes = bytesSent;
			lastUpdateTime = nowTime;
		}

		if (bytesSent - startBytes > 0 && nowTime - startTime > 0)
		{
			speed = ((bytesSent - startBytes)) / ((nowTime - startTime) / 1000.0f);
			speed = speed * 8 / 1000;
			if (speed > 5000)
			{
				wasCapped = 1;
				usleep (20000);
			}
		}

		if (startTime && nowTime - lastUpdateTime > 500)
		{
			char buff[256];
			lastUpdateTime = nowTime;

			printf ("Speed = %d kbps\n", (int)speed);

			if (nowTime - startTime > TEST_DURATION)
				break;

			wasCapped = 0;
		}
	}

abortserver:

	RTMP_DeleteStream(rtmp);

	printf ("Stream deleted.\n");

	shutdown(rtmp->m_sb.sb_socket, SHUT_WR);

	//this waits for the socket shutdown to complete gracefully
	for (;;)
	{
		char buff[1024];
		int ret;

		ret = recv(rtmp->m_sb.sb_socket, buff, sizeof(buff), 0);
		if (!ret)
			break;
		else if (ret == -1)
			break;
	}

	RTMP_Close(rtmp);
	close (rtmp->m_sb.sb_socket);

	RTMP_Free(rtmp);

	return failed;
}
