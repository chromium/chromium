// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_PLAYBACK_TTS_STREAM_H_
#define CHROMEOS_SERVICES_TTS_PLAYBACK_TTS_STREAM_H_

#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace tts {

class TtsService;

class PlaybackTtsStream : public mojom::PlaybackTtsStream {
 public:
  PlaybackTtsStream(TtsService* owner,
                    mojo::PendingReceiver<mojom::PlaybackTtsStream> receiver);
  ~PlaybackTtsStream() override;

  bool IsBound() const;

 private:
  // mojom::PlaybackTtsStream:
  void Play(PlayCallback callback) override;
  void SendAudioBuffer(const std::vector<float>& samples_buffer,
                       int32_t char_index,
                       bool is_done) override;
  void Stop() override;
  void SetVolume(float volume) override;
  void Pause() override;
  void Resume() override;

  // Owning service.
  TtsService* owner_;

  // Connection to tts in the component extension.
  mojo::Receiver<mojom::PlaybackTtsStream> stream_receiver_;
};

}  // namespace tts
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_TTS_PLAYBACK_TTS_STREAM_H_
