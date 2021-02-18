// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/proxy/assistant_proxy.h"

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "chromeos/services/assistant/proxy/conversation_controller_proxy.h"
#include "chromeos/services/assistant/proxy/libassistant_service_host.h"
#include "chromeos/services/assistant/proxy/service_controller_proxy.h"
#include "chromeos/services/libassistant/libassistant_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {
namespace assistant {

AssistantProxy::AssistantProxy() {
  background_thread_.Start();
}

AssistantProxy::~AssistantProxy() {
  StopLibassistantService();
}

void AssistantProxy::Initialize(
    LibassistantServiceHost* host,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory) {
  DCHECK(host);
  libassistant_service_host_ = host;
  LaunchLibassistantService();

  BindControllers(host, std::move(pending_url_loader_factory));
}

void AssistantProxy::LaunchLibassistantService() {
  // A Mojom service runs on the thread where its receiver was bound.
  // So to make |libassistant_service_| run on the background thread, we must
  // create it on the background thread, as it binds its receiver in its
  // constructor.
  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantProxy::LaunchLibassistantServiceOnBackgroundThread,
          // This is safe because we own the background thread,
          // so when we're deleted the background thread is stopped.
          base::Unretained(this),
          // |libassistant_service_remote_| runs on the current thread, so must
          // be bound here and not on the background thread.
          libassistant_service_remote_.BindNewPipeAndPassReceiver()));
}

void AssistantProxy::LaunchLibassistantServiceOnBackgroundThread(
    mojo::PendingReceiver<LibassistantServiceMojom> client) {
  DCHECK(background_task_runner()->BelongsToCurrentThread());
  DCHECK(libassistant_service_host_);
  libassistant_service_host_->Launch(std::move(client));
}

void AssistantProxy::StopLibassistantService() {
  // |libassistant_service_| is launched on the background thread, so we have to
  // stop it there as well.
  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantProxy::StopLibassistantServiceOnBackgroundThread,
                     base::Unretained(this)));
}

void AssistantProxy::StopLibassistantServiceOnBackgroundThread() {
  DCHECK(background_task_runner()->BelongsToCurrentThread());
  libassistant_service_host_->Stop();
}

void AssistantProxy::BindControllers(
    LibassistantServiceHost* host,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory) {
  mojo::PendingRemote<AudioInputControllerMojom>
      pending_audio_input_controller_remote;
  mojo::PendingRemote<AudioOutputDelegateMojom>
      pending_audio_output_delegate_remote;
  mojo::PendingRemote<ConversationControllerMojom>
      pending_conversation_controller_remote;
  mojo::PendingRemote<MediaDelegateMojom> pending_media_delegate_remote;
  mojo::PendingRemote<PlatformDelegateMojom> pending_platform_delegate_remote;
  mojo::PendingRemote<ServiceControllerMojom> pending_service_controller_remote;
  mojo::PendingRemote<SpeakerIdEnrollmentControllerMojom>
      pending_speaker_id_enrollment_controller_remote;

  mojo::PendingReceiver<MediaDelegateMojom> pending_media_delegate =
      pending_media_delegate_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<PlatformDelegateMojom> pending_platform_delegate =
      pending_platform_delegate_remote.InitWithNewPipeAndPassReceiver();
  pending_audio_output_delegate_receiver_ =
      pending_audio_output_delegate_remote.InitWithNewPipeAndPassReceiver();
  libassistant_service_remote_->Bind(
      pending_audio_input_controller_remote.InitWithNewPipeAndPassReceiver(),
      pending_conversation_controller_remote.InitWithNewPipeAndPassReceiver(),
      display_controller_remote_.BindNewPipeAndPassReceiver(),
      media_controller_remote_.BindNewPipeAndPassReceiver(),
      pending_service_controller_remote.InitWithNewPipeAndPassReceiver(),
      pending_speaker_id_enrollment_controller_remote
          .InitWithNewPipeAndPassReceiver(),
      std::move(pending_audio_output_delegate_remote),
      std::move(pending_media_delegate_remote),
      std::move(pending_platform_delegate_remote));

  conversation_controller_proxy_ =
      std::make_unique<ConversationControllerProxy>(
          std::move(pending_conversation_controller_remote));
  service_controller_proxy_ = std::make_unique<ServiceControllerProxy>(
      host, std::move(pending_url_loader_factory),
      std::move(pending_service_controller_remote));

  audio_input_controller_ = std::move(pending_audio_input_controller_remote);
  speaker_id_enrollment_controller_ =
      std::move(pending_speaker_id_enrollment_controller_remote);
  media_delegate_ = std::move(pending_media_delegate);
  platform_delegate_ = std::move(pending_platform_delegate);
}

scoped_refptr<base::SingleThreadTaskRunner>
AssistantProxy::background_task_runner() {
  return background_thread_.task_runner();
}

ServiceControllerProxy& AssistantProxy::service_controller() {
  DCHECK(service_controller_proxy_);
  return *service_controller_proxy_;
}

mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
AssistantProxy::ExtractAudioInputController() {
  DCHECK(audio_input_controller_.is_valid());
  return std::move(audio_input_controller_);
}

mojo::PendingReceiver<chromeos::libassistant::mojom::AudioOutputDelegate>
AssistantProxy::ExtractAudioOutputDelegate() {
  DCHECK(pending_audio_output_delegate_receiver_.is_valid());
  return std::move(pending_audio_output_delegate_receiver_);
}

mojo::PendingReceiver<chromeos::libassistant::mojom::MediaDelegate>
AssistantProxy::ExtractMediaDelegate() {
  DCHECK(media_delegate_.is_valid());
  return std::move(media_delegate_);
}

mojo::PendingReceiver<chromeos::libassistant::mojom::PlatformDelegate>
AssistantProxy::ExtractPlatformDelegate() {
  DCHECK(platform_delegate_.is_valid());
  return std::move(platform_delegate_);
}

mojo::PendingRemote<
    chromeos::libassistant::mojom::SpeakerIdEnrollmentController>
AssistantProxy::ExtractSpeakerIdEnrollmentController() {
  DCHECK(speaker_id_enrollment_controller_.is_valid());
  return std::move(speaker_id_enrollment_controller_);
}

ConversationControllerProxy& AssistantProxy::conversation_controller_proxy() {
  DCHECK(conversation_controller_proxy_);
  return *conversation_controller_proxy_;
}

AssistantProxy::DisplayController& AssistantProxy::display_controller() {
  DCHECK(display_controller_remote_.is_bound());
  return *display_controller_remote_.get();
}

AssistantProxy::MediaController& AssistantProxy::media_controller() {
  DCHECK(media_controller_remote_.is_bound());
  return *media_controller_remote_.get();
}

void AssistantProxy::AddSpeechRecognitionObserver(
    mojo::PendingRemote<
        chromeos::libassistant::mojom::SpeechRecognitionObserver> observer) {
  libassistant_service_remote_->AddSpeechRecognitionObserver(
      std::move(observer));
}

}  // namespace assistant
}  // namespace chromeos
