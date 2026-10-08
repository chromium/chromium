// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_AUDIO_RENDERER_READ_ALOUD_AUDIO_RENDERER_H_
#define CHROME_SERVICES_READALOUD_AUDIO_RENDERER_READ_ALOUD_AUDIO_RENDERER_H_

#include <atomic>
#include <cstdint>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chrome/services/readaloud/audio_renderer/word_boundary_queue.h"
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
// `media::AudioClock`, and reports each word boundary on the owning sequence
// at the moment the word becomes audible.
class ReadAloudAudioRenderer : public media::AudioRendererSink::RenderCallback {
 public:
  // Invoked on the owning sequence when a word becomes audible.
  // `start_character_offset` and `end_character_offset` are the word's
  // document offsets from its `WordTiming`, and `audio_timestamp` is the media
  // time audible at dispatch.
  using WordBoundaryCallback =
      base::RepeatingCallback<void(uint32_t start_character_offset,
                                   uint32_t end_character_offset,
                                   base::TimeDelta audio_timestamp)>;

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

  // Sets the callback run when a word boundary becomes audible.
  // Must be called on the owning sequence.
  void SetWordBoundaryCallback(WordBoundaryCallback callback);

  // Sets the playback rate for time-stretching.
  // Must be called on the owning sequence.
  void SetPlaybackRate(double rate);

  // Flushes the internal time-stretching algorithm buffer, drops every word
  // boundary not yet dispatched and restarts the media time at zero.
  // Must be called on the owning sequence.
  virtual void Flush();

  // Returns the media time of the audio currently being played out: the
  // position, in the audio consumed since the last Initialize() or Flush(),
  // that is audible now. Only advances when Render() is called, so it holds
  // still while playback is paused. Returns zero before Initialize().
  // Must be called on the owning sequence.
  base::TimeDelta GetMediaTime() const;

 private:
  // Dispatches every word boundary that has become audible, then reschedules
  // itself for the next one. Runs on the owning sequence.
  void PumpWordBoundaries();

  SEQUENCE_CHECKER(sequence_checker_);

  // Written on the owning sequence during Initialize(), read on the real-time
  // audio thread during Render(). No synchronization is needed as long as
  // Initialize() completes before Render() begins.
  media::AudioParameters params_;
  raw_ptr<AudioSegmentQueue> queue_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  bool initialized_ = false;

  // Read and written on the owning sequence (during Initialize() /
  // SetPlaybackRate()), read on the real-time audio thread during Render().
  // Using an atomic or a simple variable is safe because the parameter changes
  // are simple writes and we do not require strict synchronization.
  std::atomic<double> playback_rate_ = 1.0;

  // Only accessed on the owning sequence.
  WordBoundaryCallback word_boundary_callback_;

  media::NullMediaLog media_log_;

  // Protects the state below against concurrent accesses on the real-time
  // audio thread (Render()) and the owning sequence (e.g. Flush()). The pump
  // also takes it throughout playback, so every critical section must stay
  // cheap: no task posting, callbacks or I/O while holding it.
  mutable base::Lock lock_;
  media::AudioRendererAlgorithm algorithm_ GUARDED_BY(lock_);

  // Tracks the media time of the audio being played out. Updated by every
  // Render() with the frames written, the playback rate and the output delay
  // reported by the sink. Created in Initialize() and recreated by Flush().
  std::optional<media::AudioClock> audio_clock_ GUARDED_BY(lock_);

  // Word timings of the audio handed to `algorithm_` that have not been
  // dispatched yet, and the anchor tying media time to wall time, refreshed
  // by every Render(). Guarded by `lock_` so that each segment's audio and
  // timings are enqueued, and flushed, together.
  WordBoundaryQueue word_boundaries_ GUARDED_BY(lock_);

  // True while a PumpWordBoundaries() task is posted, so that Render() does
  // not pile up redundant ones.
  bool pump_task_posted_ GUARDED_BY(lock_) = false;

  // Bound into the pump tasks Render() posts. Render() copies it on the audio
  // thread, but it is only dereferenced on the owning sequence. Flush()
  // replaces it after invalidating `weak_factory_`, which drops pumps still
  // pending.
  base::WeakPtr<ReadAloudAudioRenderer> weak_this_ GUARDED_BY(lock_);

  base::WeakPtrFactory<ReadAloudAudioRenderer> weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_AUDIO_RENDERER_READ_ALOUD_AUDIO_RENDERER_H_
