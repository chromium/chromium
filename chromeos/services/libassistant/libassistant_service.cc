// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/libassistant_service.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "chromeos/services/assistant/public/cpp/migration/cros_platform_api.h"
#include "chromeos/services/libassistant/audio_input_controller.h"
#include "chromeos/services/libassistant/conversation_controller.h"
#include "chromeos/services/libassistant/conversation_state_listener_impl.h"
#include "chromeos/services/libassistant/display_controller.h"
#include "chromeos/services/libassistant/fake_auth_provider.h"
#include "chromeos/services/libassistant/media_controller.h"
#include "chromeos/services/libassistant/platform_api.h"
#include "chromeos/services/libassistant/service_controller.h"

namespace chromeos {
namespace libassistant {

LibassistantService::LibassistantService(
    mojo::PendingReceiver<mojom::LibassistantService> receiver,
    chromeos::assistant::CrosPlatformApi* platform_api,
    assistant::AssistantManagerServiceDelegate* delegate)
    : receiver_(this, std::move(receiver)),
      platform_api_(std::make_unique<PlatformApi>()),
      fake_auth_provider_(std::make_unique<FakeAuthProvider>()),
      audio_input_controller_(std::make_unique<AudioInputController>()),
      service_controller_(
          std::make_unique<ServiceController>(delegate, platform_api_.get())),
      conversation_controller_(
          std::make_unique<ConversationController>(service_controller_.get())),
      conversation_state_listener_(
          std::make_unique<ConversationStateListenerImpl>(
              &speech_recognition_observers_)),
      display_controller_(
          std::make_unique<DisplayController>(&speech_recognition_observers_)),
      media_controller_(std::make_unique<MediaController>()) {
  service_controller_->AddAndFireAssistantManagerObserver(
      display_controller_.get());
  service_controller_->AddAndFireAssistantManagerObserver(
      conversation_state_listener_.get());
  service_controller_->AddAndFireAssistantManagerObserver(
      media_controller_.get());

  // |platform_api| can be null during unittests.
  if (platform_api) {
    platform_api_
        ->SetAudioInputProvider(
            &audio_input_controller_->audio_input_provider())
        .SetAuthProvider(fake_auth_provider_.get())
        .SetFileProvider(&platform_api->GetFileProvider())
        .SetNetworkProvider(&platform_api->GetNetworkProvider());
  }
}

LibassistantService::~LibassistantService() {
  // We explicitly stop the Libassistant service before destroying anything,
  // to prevent use-after-free bugs.
  service_controller_->Stop();
  service_controller_->RemoveAssistantManagerObserver(
      display_controller_.get());
  service_controller_->RemoveAssistantManagerObserver(
      conversation_state_listener_.get());
  service_controller_->RemoveAssistantManagerObserver(media_controller_.get());
}

void LibassistantService::Bind(
    mojo::PendingReceiver<mojom::AudioInputController> audio_input_controller,
    mojo::PendingReceiver<mojom::ConversationController>
        conversation_controller,
    mojo::PendingReceiver<mojom::DisplayController> display_controller,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingReceiver<mojom::ServiceController> service_controller,
    mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate,
    mojo::PendingRemote<mojom::MediaDelegate> media_delegate,
    mojo::PendingRemote<mojom::PlatformDelegate> platform_delegate) {
  platform_delegate_.Bind(std::move(platform_delegate));
  audio_input_controller_->Bind(std::move(audio_input_controller),
                                platform_delegate_.get());
  conversation_controller_->Bind(std::move(conversation_controller));
  display_controller_->Bind(std::move(display_controller));
  media_controller_->Bind(std::move(media_controller),
                          std::move(media_delegate));
  platform_api_->Bind(std::move(audio_output_delegate),
                      platform_delegate_.get());
  service_controller_->Bind(std::move(service_controller));
}

void LibassistantService::SetInitializeCallback(InitializeCallback callback) {
  service_controller().SetInitializeCallback(std::move(callback));
}

void LibassistantService::AddSpeechRecognitionObserver(
    mojo::PendingRemote<mojom::SpeechRecognitionObserver> observer) {
  speech_recognition_observers_.Add(std::move(observer));
}

}  // namespace libassistant
}  // namespace chromeos
