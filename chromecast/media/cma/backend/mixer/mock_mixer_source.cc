// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mock_mixer_source.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "media/base/audio_bus.h"

using testing::_;

namespace chromecast {
namespace media {

MockMixerSource::MockMixerSource(int samples_per_second,
                                 const std::string& device_id)
    : samples_per_second_(samples_per_second), device_id_(device_id) {
  DCHECK_GT(num_channels_, 0);
  ON_CALL(*this, FillAudioPlaybackFrames(_, _, _))
      .WillByDefault(testing::Invoke(this, &MockMixerSource::GetData));
}
MockMixerSource::~MockMixerSource() = default;

void MockMixerSource::SetData(std::unique_ptr<::media::AudioBus> data) {
  data_ = std::move(data);
  data_offset_ = 0;
}

int MockMixerSource::GetData(int num_frames,
                             RenderingDelay rendering_delay,
                             ::media::AudioBus* buffer) {
  CHECK(buffer);
  CHECK_GE(buffer->frames(), num_frames);
  if (data_) {
    int frames_to_copy = std::min(num_frames, data_->frames() - data_offset_);
    data_->CopyPartialFramesTo(data_offset_, frames_to_copy, 0, buffer);
    data_offset_ += frames_to_copy;
    return frames_to_copy;
  } else {
    return 0;
  }
}

}  // namespace media
}  // namespace chromecast
