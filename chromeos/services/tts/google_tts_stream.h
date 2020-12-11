// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_GOOGLE_TTS_STREAM_H_
#define CHROMEOS_SERVICES_TTS_GOOGLE_TTS_STREAM_H_

#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "library_loaders/libchrometts.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace tts {

class TtsService;

class GoogleTtsStream : public mojom::GoogleTtsStream {
 public:
  GoogleTtsStream(TtsService* owner,
                  mojo::PendingReceiver<mojom::GoogleTtsStream> receiver);
  ~GoogleTtsStream() override;

  bool IsBound() const;

 private:
  // mojom::GoogleTtsStream:
  void InstallVoice(const std::string& voice_name,
                    const std::vector<uint8_t>& voice_bytes,
                    InstallVoiceCallback callback) override;
  void SelectVoice(const std::string& voice_name,
                   SelectVoiceCallback callback) override;
  void Speak(const std::vector<uint8_t>& text_jspb,
             const std::string& speaker_name,
             SpeakCallback callback) override;
  void Stop() override;
  void SetVolume(float volume) override;
  void Pause() override;
  void Resume() override;

  void ReadMoreFrames(bool is_first_buffer);

  // Owning service.
  TtsService* owner_;

  // Prebuilt.
  LibChromeTtsLoader libchrometts_;

  // Connection to tts in the component extension.
  mojo::Receiver<mojom::GoogleTtsStream> stream_receiver_;

  // Whether buffering is in progress.
  bool is_buffering_ = false;

  base::WeakPtrFactory<GoogleTtsStream> weak_factory_{this};
};

}  // namespace tts
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_TTS_GOOGLE_TTS_STREAM_H_
