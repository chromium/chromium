// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MOCK_MEDIA_PIPELINE_BACKEND_FOR_MIXER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MOCK_MEDIA_PIPELINE_BACKEND_FOR_MIXER_H_

#include <memory>
#include <utility>

#include "chromecast/media/cma/backend/media_pipeline_backend_for_mixer.h"
#include "chromecast/media/cma/backend/mock_audio_decoder_for_mixer.h"
#include "chromecast/media/cma/backend/mock_video_decoder_for_mixer.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

class MockMediaPipelineBackendForMixer : public MediaPipelineBackendForMixer {
 public:
  explicit MockMediaPipelineBackendForMixer(
      const MediaPipelineDeviceParams& params);

  void SetAudioDecoder(
      std::unique_ptr<MockAudioDecoderForMixer> audio_decoder) {
    audio_decoder_ = std::move(audio_decoder);
  }

  void SetVideoDecoder(std::unique_ptr<VideoDecoderForTest> video_decoder) {
    video_decoder_ = std::move(video_decoder);
  }

  int64_t MonotonicClockNow() const override;

  MockMediaPipelineBackendForMixer(const MockMediaPipelineBackendForMixer&) =
      delete;
  MockMediaPipelineBackendForMixer& operator=(
      const MockMediaPipelineBackendForMixer&) = delete;

  ~MockMediaPipelineBackendForMixer() override;
};

inline MockMediaPipelineBackendForMixer::~MockMediaPipelineBackendForMixer() =
    default;

inline MockMediaPipelineBackendForMixer::MockMediaPipelineBackendForMixer(
    const MediaPipelineDeviceParams& params)
    : MediaPipelineBackendForMixer(params) {}

inline int64_t MockMediaPipelineBackendForMixer::MonotonicClockNow() const {
  return (static_cast<base::TestMockTimeTaskRunner*>(GetTaskRunner().get())
              ->NowTicks() -
          base::TimeTicks())
      .InMicroseconds();  // 'now'...
}
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MOCK_MEDIA_PIPELINE_BACKEND_FOR_MIXER_H_
