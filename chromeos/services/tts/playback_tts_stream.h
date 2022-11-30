// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_PLAYBACK_TTS_STREAM_H_
#define CHROMEOS_SERVICES_TTS_PLAYBACK_TTS_STREAM_H_

#include "chromeos/services/tts/public/mojom/tts_service.mojom.h"
#include "chromeos/services/tts/tts_player.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace tts {

class TtsService;

class PlaybackTtsStream : public mojom::PlaybackTtsStream {
 public:
  PlaybackTtsStream(
      TtsService* owner,
      mojo::PendingReceiver<mojom::PlaybackTtsStream> receiver,
      mojo::PendingRemote<media::mojom::AudioStreamFactory> factory,
      const media::AudioParameters& params);
  ~PlaybackTtsStream() override;

  bool IsBound() const;

  TtsPlayer* tts_player_for_testing() { return &tts_player_; }

  void FlushForTesting() { stream_receiver_.FlushForTesting(); }

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

  // Connection to tts in the component extension.
  mojo::Receiver<mojom::PlaybackTtsStream> stream_receiver_;

  // Plays raw tts audio samples.
  TtsPlayer tts_player_;
};

}  // namespace tts
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_TTS_PLAYBACK_TTS_STREAM_H_
