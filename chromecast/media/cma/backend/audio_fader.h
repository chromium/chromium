// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_FADER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_FADER_H_

#include <cstdint>
#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {
namespace media {

// AudioFader handles smoothly fading audio in/out when a stream underruns
// (ie, when the data source does not have any data to provide when the output
// requests it). This prevents pops and clicks. Internally, it buffers enough
// data to ensure that a full fade can always take place if necessary; note that
// this increases output latency by |fade_frames| samples. All methods except
// constructor/destructor must be called on the same thread.
class AudioFader {
 public:
  using RenderingDelay = MediaPipelineBackend::AudioDecoder::RenderingDelay;

  // The source of real audio data for the fader.
  class Source {
   public:
    // Called to get more audio data for playback. The source must fill in
    // the |channels| with up to |num_frames| of audio. Note that only planar
    // float format is supported. The |rendering_delay| indicates when the
    // first frame of the filled data will be played out.
    // Note that this method is called on a high priority audio output thread
    // and must not block.
    // Returns the number of frames filled.
    virtual int FillFaderFrames(int num_frames,
                                RenderingDelay rendering_delay,
                                float* const* channels) = 0;

   protected:
    virtual ~Source() = default;
  };

  AudioFader(Source* source,
             base::TimeDelta fade_time,
             int num_channels,
             int sample_rate,
             double playback_rate);
  AudioFader(Source* source,
             int fade_frames,
             int num_channels,
             int sample_rate,
             double playback_rate);
  ~AudioFader();

  int buffered_frames() const { return buffered_frames_; }

  // Fills in |channel_data| with |num_frames| frames of properly faded audio.
  // The |rendering_delay| should reflect when the first sample of the filled
  // audio is expected to play out.
  int FillFrames(int num_frames,
                 RenderingDelay rendering_delay,
                 float* const* channel_data);

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
                           int num_channels,
                           int filled_frames,
                           int fade_frames,
                           int fade_frames_remaining);
  static void FadeOutHelper(float* const* channel_data,
                            int num_channels,
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

  Source* const source_;
  const int fade_frames_;
  const int num_channels_;
  const int sample_rate_;
  const double playback_rate_;

  State state_ = State::kSilent;
  std::unique_ptr<::media::AudioBus> fade_buffer_;
  int buffered_frames_ = 0;
  int fade_frames_remaining_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AudioFader);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_FADER_H_
