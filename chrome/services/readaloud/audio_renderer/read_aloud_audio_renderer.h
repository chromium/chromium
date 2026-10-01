// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_AUDIO_RENDERER_READ_ALOUD_AUDIO_RENDERER_H_
#define CHROME_SERVICES_READALOUD_AUDIO_RENDERER_READ_ALOUD_AUDIO_RENDERER_H_

#include <atomic>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_util.h"
#include "media/filters/audio_clock.h"
#include "media/filters/audio_renderer_algorithm.h"

namespace media {
class AudioBus;
struct AudioGlitchInfo;
}  // namespace media

namespace readaloud {

class AudioSegmentQueue;

// Handles rendering of decoded audio segments for ReadAloud playback.
// Implements the RenderCallback interface, which is driven by the real-time
// audio thread. Tracks the media time of the audio being played out with a
// `media::AudioClock`.
class ReadAloudAudioRenderer : public media::AudioRendererSink::RenderCallback {
 public:
  // Upper bound on the output delay fed to the audio clock. Used to clamp
  // the delay reported by the audio sink.
  static constexpr base::TimeDelta kMaxAcceptableDelay = base::Seconds(5);

  ReadAloudAudioRenderer();

  ReadAloudAudioRenderer(const ReadAloudAudioRenderer&) = delete;
  ReadAloudAudioRenderer& operator=(const ReadAloudAudioRenderer&) = delete;

  ~ReadAloudAudioRenderer() override;

  // Initializes the renderer with the target output parameters and the segment
  // queue. Must be called on the owning sequence before Render() is invoked.
  // Returns true if initialization succeeded, or false if parameters are
  // invalid.
  bool Initialize(const media::AudioParameters& params,
                  AudioSegmentQueue* queue);

  // media::AudioRendererSink::RenderCallback implementation:
  // Runs on the real-time audio thread.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const media::AudioGlitchInfo& glitch_info,
             media::AudioBus* dest) override;

  void OnRenderError() override;

  // Sets the playback rate for time-stretching.
  // Must be called on the owning sequence.
  void SetPlaybackRate(double rate);

  // Flushes the internal time-stretching algorithm buffer and restarts the
  // media time at zero.
  // Must be called on the owning sequence.
  virtual void Flush();

  // Returns the media time of the audio currently being played out: the
  // position, in the audio consumed since the last Initialize() or Flush(),
  // that is audible now. Only advances when Render() is called, so it holds
  // still while playback is paused. Returns zero before Initialize().
  // Must be called on the owning sequence.
  base::TimeDelta GetMediaTime() const;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Written on the owning sequence during Initialize(), read on the real-time
  // audio thread during Render(). No synchronization is needed as long as
  // Initialize() completes before Render() begins.
  media::AudioParameters params_;
  raw_ptr<AudioSegmentQueue> queue_ = nullptr;
  bool initialized_ = false;

  // Read and written on the owning sequence (during Initialize() /
  // SetPlaybackRate()), read on the real-time audio thread during Render().
  // Using an atomic or a simple variable is safe because the parameter changes
  // are simple writes and we do not require strict synchronization.
  std::atomic<double> playback_rate_ = 1.0;

  media::NullMediaLog media_log_;

  // Protects the state below against concurrent accesses on the real-time
  // audio thread (Render()) and the owning sequence (e.g. Flush()).
  mutable base::Lock lock_;
  media::AudioRendererAlgorithm algorithm_ GUARDED_BY(lock_);

  // Tracks the media time of the audio being played out. Updated by every
  // Render() with the frames written, the playback rate and the output delay
  // reported by the sink. Created in Initialize() and recreated by Flush().
  std::optional<media::AudioClock> audio_clock_ GUARDED_BY(lock_);
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_AUDIO_RENDERER_READ_ALOUD_AUDIO_RENDERER_H_
