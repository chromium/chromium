// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_CLOCK_SIMULATOR_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_CLOCK_SIMULATOR_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "chromecast/media/audio/audio_provider.h"
#include "chromecast/media/audio/cast_audio_bus.h"

namespace chromecast {
namespace media {

// Simulates a modifiable audio output clock rate by interpolating as needed to
// add or remove single frames.
class AudioClockSimulator : public AudioProvider {
 public:
  // Number of frames to linearly interpolate over. One interpolation starts,
  // it continues until the window is complete; rate changes only take effect
  // once the window is complete.
  static constexpr int kInterpolateWindow = 1024;

  // Maximum/minimum rate that this class supports. If the rate passed to
  // SetRate() is outside of these bounds, it is clamped to within the bounds
  // and the clamped value is returned. Note that the rate is essentially
  // (num_input_frames) / (num_output_frames).
  static constexpr double kMaxRate =
      (kInterpolateWindow + 1.0) / kInterpolateWindow;
  static constexpr double kMinRate =
      kInterpolateWindow / (kInterpolateWindow + 1.0);

  // Maximum channels that this class supports.
  static constexpr size_t kMaxChannels = 32;

  explicit AudioClockSimulator(AudioProvider* provider);
  ~AudioClockSimulator() override;

  AudioClockSimulator(const AudioClockSimulator&) = delete;
  AudioClockSimulator& operator=(const AudioClockSimulator&) = delete;

  // Sets the simulated audio clock rate. The rate is capped internally between
  // kMinRate and kMaxRate. Returns the capped effective rate.
  double SetRate(double rate);

  // Returns the number of frames of additional delay due to audio stored
  // internally. Will always return 0 or 1.
  int DelayFrames() const;

  // Sets a new playback sample rate. Needed to calculate timestamps correctly.
  void SetSampleRate(int sample_rate);

  // Sets the playback rate (rate at which samples are played out relative to
  // the sample rate). Needed to calculate timestamps correctly.
  void SetPlaybackRate(double playback_rate);

  // AudioProvider implementation:
  int FillFrames(int num_frames,
                 int64_t playout_timestamp,
                 float* const* channel_data) override;
  size_t num_channels() const override;
  int sample_rate() const override;

 private:
  enum class State {
    kPassthrough,
    kLengthening,
    kShortening,
  };

  struct FillResult {
    bool complete;
    int filled;
  };

  FillResult FillDataLengthen(int num_frames,
                              int64_t playout_timestamp,
                              float* const* channel_data,
                              int offset);
  FillResult FillDataShorten(int num_frames,
                             int64_t playout_timestamp,
                             float* const* channel_data,
                             int offset);
  void InterpolateLonger(int num_frames,
                         float* const* channel_data,
                         int offset);
  void InterpolateShorter(int num_frames,
                          float* const* channel_data,
                          int offset);

  int64_t FramesToMicroseconds(int64_t frames);

  AudioProvider* const provider_;
  int sample_rate_;
  const size_t num_channels_;
  double playback_rate_ = 1.0;

  double clock_rate_ = 1.0;
  int64_t input_frames_ = 0;
  int64_t output_frames_ = 0;

  State state_ = State::kPassthrough;
  int interpolate_position_ = 0;
  bool first_frame_filled_ = false;

  // Scratch buffer used to hold the filled original data before it is
  // interpolated for output. The first frame (first sample across all channels)
  // at index 0 is used to hold the last frame of the previous partial
  // interpolation step.
  // At the start of the interpolation window:
  //   * If we are interpolating to make the audio longer, the first frame is
  //     not used (the weight is 0), so it can be any value.
  //   * If we are interpolating to make the audio shorted, the first fill
  //     request to the provider fills in the first frame (so we request one
  //     extra frame of audio) - this accounts for the frame that is removed due
  //     to interpolation.
  // The |first_frame_filled_| member var tracks whether or not the first frame
  // of the |scratch_buffer_| contains valid audio.
  std::unique_ptr<CastAudioBus> scratch_buffer_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_CLOCK_SIMULATOR_H_
