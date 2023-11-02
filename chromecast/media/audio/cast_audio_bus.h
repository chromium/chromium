// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_BUS_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_BUS_H_

#include <memory>
#include <vector>

namespace chromecast {
namespace media {

// This class is a simplified version of ::media::AudioBus without any
// dependency on //media.
class CastAudioBus {
 public:
  // Creates a new CastAudioBus and allocates |channels| of length |frames|.
  static std::unique_ptr<CastAudioBus> Create(int channels, int frames);

  // Returns a raw pointer to the requested channel.
  float* channel(int channel) { return channel_data_[channel]; }
  const float* channel(int channel) const { return channel_data_[channel]; }

  // Returns the number of channels.
  int channels() const { return static_cast<int>(channel_data_.size()); }
  // Returns the number of frames.
  int frames() const { return frames_; }

  // Helper method for zeroing out all channels of audio data.
  void Zero();

  ~CastAudioBus();

 private:
  CastAudioBus(int channels, int frames);

  // Contiguous block of channel memory.
  std::unique_ptr<float[]> data_;

  // One float pointer per channel pointing to a contiguous block of memory for
  // that channel.
  std::vector<float*> channel_data_;
  int frames_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_BUS_H_
