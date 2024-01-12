// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_GOOGLE_TTS_STREAM_H_
#define CHROMEOS_SERVICES_TTS_GOOGLE_TTS_STREAM_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "chromeos/services/tts/tts_player.h"
#include "library_loaders/libchrometts.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace tts {

class TtsService;

class GoogleTtsStream : public mojom::GoogleTtsStream {
 public:
  GoogleTtsStream(
      TtsService* owner,
      mojo::PendingReceiver<mojom::GoogleTtsStream> receiver,
      mojo::PendingRemote<media::mojom::AudioStreamFactory> factory);
  ~GoogleTtsStream() override;

  bool IsBound() const;

  void set_is_in_process_teardown(bool value) {
    is_in_process_teardown_ = value;
  }

 private:
  // mojom::GoogleTtsStream:
  void InstallVoice(const std::string& voice_name,
                    const std::vector<uint8_t>& voice_bytes,
                    InstallVoiceCallback callback) override;
  void SelectVoice(const std::string& voice_name,
                   SelectVoiceCallback callback) override;
  void Speak(const std::vector<uint8_t>& text_jspb,
             const std::vector<uint8_t>& speaker_params_jspb,
             SpeakCallback callback) override;
  void Stop() override;
  void SetVolume(float volume) override;
  void Pause() override;
  void Resume() override;

  void ReadMoreFrames(bool is_first_buffer);

  // Owning service.
  raw_ptr<TtsService> owner_;

  // Prebuilt.
  LibChromeTtsLoader libchrometts_;

  // Connection to tts in the component extension.
  mojo::Receiver<mojom::GoogleTtsStream> stream_receiver_;

  // Whether buffering is in progress.
  bool is_buffering_ = false;

  // Plays raw tts audio samples.
  TtsPlayer tts_player_;

  // Whether the tts service process is tearing down.
  bool is_in_process_teardown_ = false;

  base::WeakPtrFactory<GoogleTtsStream> weak_factory_{this};
};

}  // namespace tts
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_TTS_GOOGLE_TTS_STREAM_H_
