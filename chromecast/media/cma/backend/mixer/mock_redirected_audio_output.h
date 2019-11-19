// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MOCK_REDIRECTED_AUDIO_OUTPUT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MOCK_REDIRECTED_AUDIO_OUTPUT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "chromecast/media/audio/mixer_service/redirected_audio_connection.h"
#include "chromecast/public/volume_control.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {

class MockRedirectedAudioOutput
    : public mixer_service::RedirectedAudioConnection::Delegate {
 public:
  explicit MockRedirectedAudioOutput(
      const mixer_service::RedirectedAudioConnection::Config& config);
  ~MockRedirectedAudioOutput() override;

  ::media::AudioBus* last_buffer() const { return last_buffer_.get(); }
  int64_t last_output_timestamp() const { return last_output_timestamp_; }

  MOCK_METHOD4(OnRedirectedAudio, void(int64_t, int, float*, int));

  void SetStreamMatchPatterns(
      std::vector<std::pair<AudioContentType, std::string>> patterns);

 private:
  void HandleRedirectedAudio(int64_t timestamp,
                             int sample_rate,
                             float* data,
                             int frames);

  const mixer_service::RedirectedAudioConnection::Config config_;
  mixer_service::RedirectedAudioConnection connection_;

  std::unique_ptr<::media::AudioBus> last_buffer_;
  int64_t last_output_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(MockRedirectedAudioOutput);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MOCK_REDIRECTED_AUDIO_OUTPUT_H_
