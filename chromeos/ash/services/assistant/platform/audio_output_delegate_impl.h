// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_AUDIO_OUTPUT_DELEGATE_IMPL_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_AUDIO_OUTPUT_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_output_delegate.mojom.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::assistant {

class AssistantMediaSession;

class AudioOutputDelegateImpl
    : public libassistant::mojom::AudioOutputDelegate {
 public:
  explicit AudioOutputDelegateImpl(AssistantMediaSession* media_session);
  AudioOutputDelegateImpl(const AudioOutputDelegateImpl&) = delete;
  AudioOutputDelegateImpl& operator=(const AudioOutputDelegateImpl&) = delete;
  ~AudioOutputDelegateImpl() override;

  void Bind(mojo::PendingReceiver<AudioOutputDelegate> pending_receiver);
  void Stop();

  // libassistant::mojom::AudioOutputDelegate implementation:
  void RequestAudioFocus(
      libassistant::mojom::AudioOutputStreamType stream_type) override;
  void AbandonAudioFocusIfNeeded() override;
  void AddMediaSessionObserver(
      mojo::PendingRemote<::media_session::mojom::MediaSessionObserver>
          observer) override;

 private:
  mojo::Receiver<AudioOutputDelegate> receiver_{this};
  const raw_ptr<AssistantMediaSession> media_session_;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_AUDIO_OUTPUT_DELEGATE_IMPL_H_
