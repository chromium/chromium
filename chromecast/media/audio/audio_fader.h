// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_FADER_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_FADER_H_

#include <cstdint>
#include <memory>

#include "base/time/time.h"
#include "chromecast/media/api/audio_provider.h"
#include "chromecast/media/audio/cast_audio_bus.h"

namespace chromecast {
namespace media {

// AudioFader handles smoothly fading audio in/out when a stream underruns
// (ie, when the data source does not have any data to provide when the output
// requests it). This prevents pops and clicks. Internally, it buffers enough
// data to ensure that a full fade can always take place if necessary; note that
// this increases output latency by |fade_frames| samples. All methods except
// constructor/destructor must be called on the same thread.
class AudioFader : public AudioProvider {
 public:
  AudioFader(AudioProvider* provider,
             base::TimeDelta fade_time,
             double playback_rate);
  AudioFader(AudioProvider* provider, int fade_frames, double playback_rate);

  AudioFader(const AudioFader&) = delete;
  AudioFader& operator=(const AudioFader&) = delete;

  ~AudioFader() override;

  int buffered_frames() const { return buffered_frames_; }

  // AudioProvider implementation:
  int FillFrames(int num_frames,
                 int64_t playout_timestamp,
                 float* const* channel_data) override;
  size_t num_channels() const override;
  int sample_rate() const override;

  void set_playback_rate(double playback_rate) {
    playback_rate_ = playback_rate;
  }

  // Returns the total number of frames that will be requested from the source
  // (potentially over multiple calls to source_->FillFaderFrames()) if
  // FillFrames() is called to fill |num_fill_frames| frames.
  int FramesNeededFromSource(int num_fill_frames) const;

  // Helper methods to fade in/out a buffer. |channel_data| contains the data to
  // fade; |filled_frames| is the amount of data actually in |channel_data|.
  // |fade_frames| is the number of frames over which a complete fade should
  // happen (ie, how many frames it takes to go from a 1.0 to 0.0 multiplier).
  // |fade_frames_remaining| is the number of frames left in the current fade
  // (which will be less than |fade_frames| if part of the fade has already
  // been completed on a previous buffer).
  static void FadeInHelper(float* const* channel_data,
                           size_t num_channels,
                           int filled_frames,
                           int fade_frames,
                           int fade_frames_remaining);
  static void FadeOutHelper(float* const* channel_data,
                            size_t num_channels,
                            int filled_frames,
                            int fade_frames,
                            int fade_frames_remaining);

 private:
  enum class State {
    kSilent,
    kFadingIn,
    kPlaying,
    kFadingOut,
  };

  int64_t FramesToMicroseconds(int64_t frames);

  void CompleteFill(float* const* channel_data, int filled_frames);
  void IncompleteFill(float* const* channel_data, int filled_frames);
  void FadeIn(float* const* channel_data, int filled_frames);
  void FadeOut(float* const* channel_data, int filled_frames);

  AudioProvider* const provider_;
  const int fade_frames_;
  const size_t num_channels_;
  const int sample_rate_;
  double playback_rate_;

  State state_ = State::kSilent;
  std::unique_ptr<CastAudioBus> fade_buffer_;
  int buffered_frames_ = 0;
  int fade_frames_remaining_ = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_FADER_H_
