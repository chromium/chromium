// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_TTS_SERVICE_H_
#define CHROMEOS_SERVICES_TTS_TTS_SERVICE_H_

#include "chromeos/services/tts/google_tts_stream.h"
#include "chromeos/services/tts/playback_tts_stream.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace tts {

class TtsService : public mojom::TtsService {
 public:
  explicit TtsService(mojo::PendingReceiver<mojom::TtsService> receiver);
  ~TtsService() override;

  // Maybe exit this process.
  void MaybeExit();

  void set_keep_process_alive_for_testing(bool value) {
    keep_process_alive_for_testing_ = value;
  }

  mojo::Receiver<mojom::TtsService>* receiver_for_testing() {
    return &service_receiver_;
  }

  PlaybackTtsStream* playback_tts_stream_for_testing() {
    return playback_tts_stream_.get();
  }

  // mojom::TtsService:
  void BindGoogleTtsStream(
      mojo::PendingReceiver<mojom::GoogleTtsStream> receiver,
      mojo::PendingRemote<media::mojom::AudioStreamFactory> factory) override;
  void BindPlaybackTtsStream(
      mojo::PendingReceiver<mojom::PlaybackTtsStream> receiver,
      mojo::PendingRemote<media::mojom::AudioStreamFactory> factory,
      mojom::AudioParametersPtr desired_audio_parameters,
      BindPlaybackTtsStreamCallback callback) override;

 private:
  // Connection to tts in the browser.
  mojo::Receiver<mojom::TtsService> service_receiver_;

  // The Google text-to-speech engine.
  std::unique_ptr<GoogleTtsStream> google_tts_stream_;

  // The active playback-based text-to-speech engine.
  std::unique_ptr<PlaybackTtsStream> playback_tts_stream_;

  // Keeps this process alive for testing.
  bool keep_process_alive_for_testing_ = false;

  base::WeakPtrFactory<TtsService> weak_factory_{this};
};

}  // namespace tts
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_TTS_TTS_SERVICE_H_
