// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_TTS_PLAYER_H_
#define CHROMEOS_SERVICES_TTS_TTS_PLAYER_H_

#include <queue>

#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_renderer_sink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/cpp/output_device.h"

namespace chromeos {
namespace tts {

class TtsPlayer : public media::AudioRendererSink::RenderCallback {
 public:
  typedef std::pair<int, base::TimeDelta> Timepoint;

  // Helper group of state to pass from main thread to audio thread.
  struct AudioBuffer {
    AudioBuffer();
    ~AudioBuffer();
    AudioBuffer(const AudioBuffer& other) = delete;
    AudioBuffer(AudioBuffer&& other);
    AudioBuffer& operator=(AudioBuffer&& other) = default;

    std::vector<float> frames;
    int char_index = -1;
    int status = 0;
    bool is_first_buffer = false;

    // Internal bookkeeping if only a partial buffer has been read during
    // TtsPlayer::Render.
    size_t current_frame_index = 0;
  };

  TtsPlayer(mojo::PendingRemote<media::mojom::AudioStreamFactory> factory,
            const media::AudioParameters& params);
  ~TtsPlayer() override;

  // Audio operations.
  void Play(
      base::OnceCallback<void(mojo::PendingReceiver<mojom::TtsEventObserver>)>
          callback);
  void AddAudioBuffer(AudioBuffer buf);
  void AddExplicitTimepoint(int char_index, base::TimeDelta delay);
  void Stop();
  void SetVolume(float volume);
  void Pause();
  void Resume();

  // media::AudioRendererSink::RenderCallback:
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const media::AudioGlitchInfo& glitch_info,
             media::AudioBus* dest) override;
  void OnRenderError() override;

 private:
  // Handles stopping tts.
  void StopLocked(bool clear_buffers = true)
      EXCLUSIVE_LOCKS_REQUIRED(state_lock_);

  // Do any processing (e.g. sending start/end events) on buffers that have just
  // been rendered on the audio thread.
  void ProcessRenderedBuffers();

  // Post task on the main thread to call ProcessRenderedBuffers.
  void PostTaskProcessRenderedBuffersLocked(AudioBuffer* buffer)
      EXCLUSIVE_LOCKS_REQUIRED(state_lock_);

  // Protects access to state from main thread and audio thread.
  base::Lock state_lock_;

  // Connection to send tts events.
  mojo::Remote<mojom::TtsEventObserver> tts_event_observer_;

  // Outputs speech synthesis to audio.
  audio::OutputDevice output_device_;

  // The queue of audio buffers to be played by the audio thread.
  std::queue<AudioBuffer> buffers_ GUARDED_BY(state_lock_);
  std::queue<AudioBuffer> rendered_buffers_;

  // An explicit list of increasing time delta sorted timepoints to be fired
  // while rendering audio at the specified |delay| from start of audio
  // playback. An AudioBuffer may contain an implicit timepoint for callers who
  // specify a character index along with the audio buffer.
  std::queue<Timepoint> timepoints_ GUARDED_BY(state_lock_);

  // The time at which playback of the current utterance started.
  base::Time start_playback_time_;

  // Whether a task to process rendered audio buffers has been posted.
  bool process_rendered_buffers_posted_ GUARDED_BY(state_lock_) = false;

  // Handles tasks posted from the audio thread, processed in the main thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<TtsPlayer> weak_factory_{this};
};

}  // namespace tts
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_TTS_TTS_PLAYER_H_
