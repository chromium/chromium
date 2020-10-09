// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_TTS_SERVICE_H_
#define CHROMEOS_SERVICES_TTS_TTS_SERVICE_H_

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
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
                   public mojom::TtsStream,
                   public media::AudioRendererSink::RenderCallback {
 public:
  explicit TtsService(mojo::PendingReceiver<mojom::TtsService> receiver);
  ~TtsService() override;

 private:
  // mojom::TtsService:
  void BindTtsStream(
      mojo::PendingReceiver<mojom::TtsStream> receiver,
      mojo::PendingRemote<audio::mojom::StreamFactory> factory) override;

  // mojom::TtsStream:
  void InstallVoice(const std::string& voice_name,
                    const std::vector<uint8_t>& voice_bytes,
                    InstallVoiceCallback callback) override;
  void SelectVoice(const std::string& voice_name,
                   SelectVoiceCallback callback) override;
  void Speak(const std::vector<uint8_t>& text_jspb,
             SpeakCallback callback) override;
  void Stop() override;
  void SetVolume(float volume) override;

  // media::AudioRendererSink::RenderCallback:
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             int prior_frames_skipped,
             media::AudioBus* dest) override;
  void OnRenderError() override;

  // Handles stopping tts.
  void StopLocked() EXCLUSIVE_LOCKS_REQUIRED(state_lock_);

  // Connection to tts in the browser.
  mojo::Receiver<mojom::TtsService> service_receiver_;

  // Protects access to state from main thread and audio thread.
  base::Lock state_lock_;

  // Prebuilt.
  LibChromeTtsLoader libchrometts_ GUARDED_BY(state_lock_);

  // Connection to tts in the component extension.
  mojo::Receiver<mojom::TtsStream> stream_receiver_ GUARDED_BY(state_lock_);

  // Connection to send tts events to component extension.
  mojo::Remote<mojom::TtsEventObserver> tts_event_observer_
      GUARDED_BY(state_lock_);

  // Outputs speech synthesis to audio.
  std::unique_ptr<audio::OutputDevice> output_device_ GUARDED_BY(state_lock_);

  // Tracks whether any audio data came as a result of |Speak|. Reset for every
  // call to |Speak|.
  bool got_first_buffer_ GUARDED_BY(state_lock_);

  // The first buffer; used for prefetching/warming up the engine for a new
  // utterance. The first item is the audio data, the second is the status
  // returned by a call to GoogleTtsReadBuffered.
  std::pair<std::vector<float>, size_t> first_buf_;
};

}  // namespace tts
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_TTS_TTS_SERVICE_H_
