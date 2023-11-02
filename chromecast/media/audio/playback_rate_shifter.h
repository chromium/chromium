// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_PLAYBACK_RATE_SHIFTER_H_
#define CHROMECAST_MEDIA_AUDIO_PLAYBACK_RATE_SHIFTER_H_

#include <cstdint>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chromecast/media/api/audio_provider.h"
#include "media/base/channel_layout.h"
#include "media/base/media_util.h"

namespace media {
class AudioBufferMemoryPool;
class AudioBus;
class AudioRendererAlgorithm;
}  // namespace media

namespace chromecast {
namespace media {

// PlaybackRateShifter handles shifting playback rate (eg 0.25x to 4x playback).
// All methods except constructor/destructor must be called on the same thread.
// A fader should be chained after the PlaybackRateShifter, since there can be
// audio discontinuities when the playback rate is changed.
class PlaybackRateShifter : public AudioProvider {
 public:
  PlaybackRateShifter(AudioProvider* provider,
                      ::media::ChannelLayout channel_layout,
                      int num_channels,
                      int sample_rate,
                      int request_size);
  ~PlaybackRateShifter() override;

  // Returns the effective number of frames buffered, relative to the output
  // playback rate.
  double BufferedFrames() const;

  // Changes the playback rate.
  void SetPlaybackRate(double rate);

  // AudioProvider implementation:
  int FillFrames(int num_frames,
                 int64_t playout_timestamp,
                 float* const* channel_data) override;
  size_t num_channels() const override;
  int sample_rate() const override;

  double playback_rate() const { return playback_rate_; }

 private:
  int DrainBufferedData(int num_frames,
                        int64_t playout_timestamp,
                        float* const* channel_data);
  int64_t FramesToMicroseconds(double frames);

  AudioProvider* const provider_;
  const ::media::ChannelLayout channel_layout_;
  const size_t num_channels_;
  const int sample_rate_;
  const int request_size_;

  double playback_rate_ = 1.0;

  ::media::NullMediaLog media_log_;
  std::unique_ptr<::media::AudioRendererAlgorithm> rate_shifter_;
  std::unique_ptr<::media::AudioBus> rate_shifter_output_;
  scoped_refptr<::media::AudioBufferMemoryPool> audio_buffer_pool_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_PLAYBACK_RATE_SHIFTER_H_
