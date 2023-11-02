// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_TEST_MOCK_CMA_BACKEND_H_
#define CHROMECAST_MEDIA_API_TEST_MOCK_CMA_BACKEND_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/public/graphics_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace media {

class MockCmaBackend : public CmaBackend {
 public:
  class DecoderDelegate : public CmaBackend::Decoder::Delegate {
   public:
    DecoderDelegate();
    ~DecoderDelegate() override;
    MOCK_METHOD1(OnPushBufferComplete, void(BufferStatus));
    MOCK_METHOD0(OnEndOfStream, void());
    MOCK_METHOD0(OnDecoderError, void());
    MOCK_METHOD3(OnKeyStatusChanged,
                 void(const std::string&, CastKeyStatus, uint32_t));
    MOCK_METHOD1(OnVideoResolutionChanged, void(const Size&));
  };

  class AudioDecoder : public CmaBackend::AudioDecoder {
   public:
    AudioDecoder();
    ~AudioDecoder() override;
    MOCK_METHOD1(SetDelegate, void(Delegate*));
    MOCK_METHOD1(PushBuffer, BufferStatus(scoped_refptr<DecoderBufferBase>));
    MOCK_METHOD1(SetConfig, bool(const AudioConfig&));
    MOCK_METHOD1(SetVolume, bool(float));
    MOCK_METHOD0(GetRenderingDelay, RenderingDelay());
    MOCK_METHOD1(GetStatistics, void(Statistics*));
    MOCK_METHOD0(GetAudioTrackTimestamp, AudioTrackTimestamp());
    MOCK_METHOD0(GetStartThresholdInFrames, int());
    MOCK_METHOD0(RequiresDecryption, bool());
  };

  class VideoDecoder : public CmaBackend::VideoDecoder {
   public:
    VideoDecoder();
    ~VideoDecoder() override;
    MOCK_METHOD1(SetDelegate, void(Delegate*));
    MOCK_METHOD1(PushBuffer, BufferStatus(scoped_refptr<DecoderBufferBase>));
    MOCK_METHOD1(SetConfig, bool(const VideoConfig&));
    MOCK_METHOD1(GetStatistics, void(Statistics*));
  };

  MockCmaBackend();
  ~MockCmaBackend() override;
  MOCK_METHOD0(CreateAudioDecoder, AudioDecoder*());
  MOCK_METHOD0(CreateVideoDecoder, VideoDecoder*());
  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD1(Start, bool(int64_t));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Pause, bool());
  MOCK_METHOD0(Resume, bool());
  MOCK_METHOD0(GetCurrentPts, int64_t());
  MOCK_METHOD1(SetPlaybackRate, bool(float));
  MOCK_METHOD0(LogicalPause, void());
  MOCK_METHOD0(LogicalResume, void());
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_TEST_MOCK_CMA_BACKEND_H_
