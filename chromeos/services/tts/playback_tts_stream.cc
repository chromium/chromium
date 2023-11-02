// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/playback_tts_stream.h"

#include "chromeos/services/tts/constants.h"
#include "chromeos/services/tts/tts_service.h"

namespace chromeos {
namespace tts {

PlaybackTtsStream::PlaybackTtsStream(
    TtsService* owner,
    mojo::PendingReceiver<mojom::PlaybackTtsStream> receiver,
    mojo::PendingRemote<media::mojom::AudioStreamFactory> factory,
    const media::AudioParameters& params)
    : stream_receiver_(this, std::move(receiver)),
      tts_player_(std::move(factory), params) {
  stream_receiver_.set_disconnect_handler(base::BindOnce(
      [](TtsService* owner,
         mojo::Receiver<mojom::PlaybackTtsStream>* receiver) {
        // The remote which lives in component extension js has been
        // disconnected due to destruction or error.
        receiver->reset();
        owner->MaybeExit();
      },
      owner, &stream_receiver_));
}

PlaybackTtsStream::~PlaybackTtsStream() = default;

bool PlaybackTtsStream::IsBound() const {
  return stream_receiver_.is_bound();
}

void PlaybackTtsStream::Play(PlayCallback callback) {
  tts_player_.Play(std::move(callback));

  // A small buffer to signal the start of the audio for this utterance.
  TtsPlayer::AudioBuffer buf;
  buf.frames.resize(1, 0);
  buf.status = 1;
  buf.is_first_buffer = true;
  tts_player_.AddAudioBuffer(std::move(buf));
}

void PlaybackTtsStream::SendAudioBuffer(
    const std::vector<float>& samples_buffer,
    int32_t char_index,
    bool is_done) {
  TtsPlayer::AudioBuffer buf;
  buf.frames = samples_buffer;
  buf.status = is_done ? 0 : 1;
  buf.char_index = char_index;
  tts_player_.AddAudioBuffer(std::move(buf));
}

void PlaybackTtsStream::Stop() {
  tts_player_.Stop();
}

void PlaybackTtsStream::SetVolume(float volume) {
  tts_player_.SetVolume(volume);
}

void PlaybackTtsStream::Pause() {
  tts_player_.Pause();
}

void PlaybackTtsStream::Resume() {
  tts_player_.Resume();
}

}  // namespace tts
}  // namespace chromeos
