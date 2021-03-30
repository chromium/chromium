// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/proxy/assistant_proxy.h"

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "chromeos/services/assistant/proxy/libassistant_service_host.h"

namespace chromeos {
namespace assistant {

AssistantProxy::AssistantProxy() {
  background_thread_.Start();
}

AssistantProxy::~AssistantProxy() {
  StopLibassistantService();
}

void AssistantProxy::Initialize(LibassistantServiceHost* host) {
  DCHECK(host);
  libassistant_service_host_ = host;
  LaunchLibassistantService();

  BindControllers();
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
          // |libassistant_service_| runs on the current thread, so must
          // be bound here and not on the background thread.
          libassistant_service_.BindNewPipeAndPassReceiver()));
}

void AssistantProxy::LaunchLibassistantServiceOnBackgroundThread(
    mojo::PendingReceiver<chromeos::libassistant::mojom::LibassistantService>
        client) {
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

void AssistantProxy::BindControllers() {
  mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
      pending_audio_input_controller_remote;
  mojo::PendingRemote<chromeos::libassistant::mojom::AudioOutputDelegate>
      pending_audio_output_delegate_remote;
  mojo::PendingRemote<chromeos::libassistant::mojom::DeviceSettingsDelegate>
      pending_device_settings_delegate_remote;
  mojo::PendingRemote<chromeos::libassistant::mojom::MediaDelegate>
      pending_media_delegate_remote;
  mojo::PendingRemote<chromeos::libassistant::mojom::NotificationDelegate>
      pending_notification_delegate_remote;
  mojo::PendingRemote<chromeos::libassistant::mojom::PlatformDelegate>
      pending_platform_delegate_remote;
  mojo::PendingRemote<
      chromeos::libassistant::mojom::SpeakerIdEnrollmentController>
      pending_speaker_id_enrollment_controller_remote;
  mojo::PendingRemote<chromeos::libassistant::mojom::TimerDelegate>
      pending_timer_delegate_remote;

  media_delegate_ =
      pending_media_delegate_remote.InitWithNewPipeAndPassReceiver();
  notification_delegate_ =
      pending_notification_delegate_remote.InitWithNewPipeAndPassReceiver();
  platform_delegate_ =
      pending_platform_delegate_remote.InitWithNewPipeAndPassReceiver();
  timer_delegate_ =
      pending_timer_delegate_remote.InitWithNewPipeAndPassReceiver();

  pending_audio_output_delegate_receiver_ =
      pending_audio_output_delegate_remote.InitWithNewPipeAndPassReceiver();
  pending_device_settings_delegate_receiver_ =
      pending_device_settings_delegate_remote.InitWithNewPipeAndPassReceiver();
  libassistant_service_->Bind(
      pending_audio_input_controller_remote.InitWithNewPipeAndPassReceiver(),
      conversation_controller_.BindNewPipeAndPassReceiver(),
      display_controller_.BindNewPipeAndPassReceiver(),
      media_controller_.BindNewPipeAndPassReceiver(),
      service_controller_.BindNewPipeAndPassReceiver(),
      settings_controller_.BindNewPipeAndPassReceiver(),
      pending_speaker_id_enrollment_controller_remote
          .InitWithNewPipeAndPassReceiver(),
      timer_controller_.BindNewPipeAndPassReceiver(),
      std::move(pending_audio_output_delegate_remote),
      std::move(pending_device_settings_delegate_remote),
      std::move(pending_media_delegate_remote),
      std::move(pending_notification_delegate_remote),
      std::move(pending_platform_delegate_remote),
      std::move(pending_timer_delegate_remote));

  audio_input_controller_ = std::move(pending_audio_input_controller_remote);
  speaker_id_enrollment_controller_ =
      std::move(pending_speaker_id_enrollment_controller_remote);
}

scoped_refptr<base::SingleThreadTaskRunner>
AssistantProxy::background_task_runner() {
  return background_thread_.task_runner();
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

mojo::PendingReceiver<chromeos::libassistant::mojom::DeviceSettingsDelegate>
AssistantProxy::ExtractDeviceSettingsDelegate() {
  DCHECK(pending_device_settings_delegate_receiver_.is_valid());
  return std::move(pending_device_settings_delegate_receiver_);
}

mojo::PendingReceiver<chromeos::libassistant::mojom::MediaDelegate>
AssistantProxy::ExtractMediaDelegate() {
  DCHECK(media_delegate_.is_valid());
  return std::move(media_delegate_);
}

mojo::PendingReceiver<chromeos::libassistant::mojom::NotificationDelegate>
AssistantProxy::ExtractNotificationDelegate() {
  DCHECK(notification_delegate_.is_valid());
  return std::move(notification_delegate_);
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

mojo::PendingReceiver<chromeos::libassistant::mojom::TimerDelegate>
AssistantProxy::ExtractTimerDelegate() {
  DCHECK(timer_delegate_.is_valid());
  return std::move(timer_delegate_);
}

chromeos::libassistant::mojom::ConversationController&
AssistantProxy::conversation_controller() {
  DCHECK(conversation_controller_.is_bound());
  return *conversation_controller_;
}

chromeos::libassistant::mojom::DisplayController&
AssistantProxy::display_controller() {
  DCHECK(display_controller_.is_bound());
  return *display_controller_.get();
}

chromeos::libassistant::mojom::ServiceController&
AssistantProxy::service_controller() {
  DCHECK(service_controller_.is_bound());
  return *service_controller_.get();
}

chromeos::libassistant::mojom::MediaController&
AssistantProxy::media_controller() {
  DCHECK(media_controller_.is_bound());
  return *media_controller_.get();
}

chromeos::libassistant::mojom::SettingsController&
AssistantProxy::settings_controller() {
  DCHECK(settings_controller_.is_bound());
  return *settings_controller_;
}

chromeos::libassistant::mojom::TimerController&
AssistantProxy::timer_controller() {
  DCHECK(timer_controller_.is_bound());
  return *timer_controller_.get();
}

void AssistantProxy::AddSpeechRecognitionObserver(
    mojo::PendingRemote<
        chromeos::libassistant::mojom::SpeechRecognitionObserver> observer) {
  libassistant_service_->AddSpeechRecognitionObserver(std::move(observer));
}

void AssistantProxy::AddAuthenticationStateObserver(
    mojo::PendingRemote<
        chromeos::libassistant::mojom::AuthenticationStateObserver> observer) {
  libassistant_service_->AddAuthenticationStateObserver(std::move(observer));
}

}  // namespace assistant
}  // namespace chromeos
