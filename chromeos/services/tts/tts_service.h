// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_TTS_SERVICE_H_
#define CHROMEOS_SERVICES_TTS_TTS_SERVICE_H_

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "chromeos/services/tts/google_tts_stream.h"
#include "chromeos/services/tts/playback_tts_stream.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "library_loaders/libchrometts.h"
#include "media/base/audio_renderer_sink.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace audio {
class OutputDevice;
}

namespace chromeos {
namespace tts {

class TtsService : public mojom::TtsService,
                   public mojom::TtsStreamFactory,
                   public media::AudioRendererSink::RenderCallback {
 public:
  typedef std::pair<int, base::TimeDelta> Timepoint;

  // Helper group of state to pass from main thread to audio thread.
  struct AudioBuffer {
    AudioBuffer();
    ~AudioBuffer();
    AudioBuffer(const AudioBuffer& other) = delete;
    AudioBuffer(AudioBuffer&& other);

    std::vector<float> frames;
    int char_index = -1;
    int status = 0;
    bool is_first_buffer = false;
  };

  explicit TtsService(mojo::PendingReceiver<mojom::TtsService> receiver);
  ~TtsService() override;

  // Audio operations.
  void Play(
      base::OnceCallback<void(::mojo::PendingReceiver<mojom::TtsEventObserver>)>
          callback);
  void AddAudioBuffer(AudioBuffer buf);
  void AddExplicitTimepoint(int char_index, base::TimeDelta delay);
  void Stop();
  void SetVolume(float volume);
  void Pause();
  void Resume();

  // Maybe exit this process.
  void MaybeExit();

  mojo::Receiver<mojom::TtsStreamFactory>* tts_stream_factory_for_testing() {
    return &tts_stream_factory_;
  }

  std::queue<mojo::PendingReceiver<mojom::TtsStreamFactory>>&
  pending_tts_stream_factory_receivers_for_testing() {
    return pending_tts_stream_factory_receivers_;
  }

  // mojom::TtsService:
  void BindTtsStreamFactory(
      mojo::PendingReceiver<mojom::TtsStreamFactory> receiver,
      mojo::PendingRemote<audio::mojom::StreamFactory> factory) override;

  // mojom::GoogleTtsStream:
  void CreateGoogleTtsStream(CreateGoogleTtsStreamCallback callback) override;
  void CreatePlaybackTtsStream(
      CreatePlaybackTtsStreamCallback callback) override;

  // media::AudioRendererSink::RenderCallback:
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             int prior_frames_skipped,
             media::AudioBus* dest) override;
  void OnRenderError() override;

 private:
  // Handles stopping tts.
  void StopLocked(bool clear_buffers = true)
      EXCLUSIVE_LOCKS_REQUIRED(state_lock_);

  // Satisfies any pending tts stream factory receivers.
  void ProcessPendingTtsStreamFactories();

  // Connection to tts in the browser.
  mojo::Receiver<mojom::TtsService> service_receiver_;

  // Factory creating various types of streams.
  mojo::Receiver<mojom::TtsStreamFactory> tts_stream_factory_;

  // A list of pending component extension requesting a tts stream factory.
  std::queue<mojo::PendingReceiver<mojom::TtsStreamFactory>>
      pending_tts_stream_factory_receivers_;

  std::unique_ptr<GoogleTtsStream> google_tts_stream_;

  std::unique_ptr<PlaybackTtsStream> playback_tts_stream_;

  // Protects access to state from main thread and audio thread.
  base::Lock state_lock_;

  // Connection to send tts events to component extension.
  mojo::Remote<mojom::TtsEventObserver> tts_event_observer_;

  // Outputs speech synthesis to audio.
  std::unique_ptr<audio::OutputDevice> output_device_;

  // The queue of audio buffers to be played by the audio thread.
  std::queue<AudioBuffer> buffers_ GUARDED_BY(state_lock_);

  // An explicit list of increasing time delta sorted timepoints to be fired
  // while rendering audio at the specified |delay| from start of audio
  // playback. An AudioBuffer may contain an implicit timepoint for callers who
  // specify a character index along with the audio buffer.
  std::queue<Timepoint> timepoints_ GUARDED_BY(state_lock_);

  // The time at which playback of the current utterance started.
  base::Time start_playback_time_;
};

}  // namespace tts
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_TTS_TTS_SERVICE_H_
