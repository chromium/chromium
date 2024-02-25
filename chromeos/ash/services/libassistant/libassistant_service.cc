// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/libassistant_service.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/libassistant/libassistant_factory.h"
#include "chromeos/ash/services/libassistant/libassistant_loader_impl.h"
#include "chromeos/ash/services/libassistant/public/mojom/speech_recognition_observer.mojom.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"

namespace ash::libassistant {

namespace {

class LibassistantFactoryImpl : public LibassistantFactory {
 public:
  explicit LibassistantFactoryImpl(assistant_client::PlatformApi* platform_api)
      : platform_api_(platform_api) {}
  LibassistantFactoryImpl(const LibassistantFactoryImpl&) = delete;
  LibassistantFactoryImpl& operator=(const LibassistantFactoryImpl&) = delete;
  ~LibassistantFactoryImpl() override = default;

  // LibassistantFactory implementation:
  std::unique_ptr<assistant_client::AssistantManager> CreateAssistantManager(
      const std::string& lib_assistant_config) override {
    auto* entrypoint = LibassistantLoaderImpl::GetInstance()->GetEntryPoint();
    assistant_client::AssistantManager* assistant_manager =
        entrypoint->NewAssistantManager(lib_assistant_config, platform_api_);
    return base::WrapUnique(assistant_manager);
  }

 private:
  const raw_ptr<assistant_client::PlatformApi> platform_api_;
};

std::unique_ptr<LibassistantFactory> FactoryOrDefault(
    std::unique_ptr<LibassistantFactory> factory,
    assistant_client::PlatformApi* platform_api) {
  if (factory)
    return factory;

  return std::make_unique<LibassistantFactoryImpl>(platform_api);
}

}  // namespace

LibassistantService::LibassistantService(
    mojo::PendingReceiver<mojom::LibassistantService> receiver,
    std::unique_ptr<LibassistantFactory> factory)
    : receiver_(this, std::move(receiver)),
      libassistant_factory_(
          FactoryOrDefault(std::move(factory), &platform_api_)),
      service_controller_(libassistant_factory_.get()),
      conversation_state_listener_(
          &speech_recognition_observers_,
          conversation_controller_.conversation_observers(),
          &audio_input_controller_),
      display_controller_(&speech_recognition_observers_),
      speaker_id_enrollment_controller_(&audio_input_controller_) {
  service_controller_.AddAndFireAssistantClientObserver(&platform_api_);
  service_controller_.AddAndFireAssistantClientObserver(
      &conversation_controller_);
  service_controller_.AddAndFireAssistantClientObserver(
      &conversation_state_listener_);
  service_controller_.AddAndFireAssistantClientObserver(
      &device_settings_controller_);
  service_controller_.AddAndFireAssistantClientObserver(&display_controller_);
  service_controller_.AddAndFireAssistantClientObserver(&media_controller_);
  service_controller_.AddAndFireAssistantClientObserver(
      &speaker_id_enrollment_controller_);
  service_controller_.AddAndFireAssistantClientObserver(&settings_controller_);
  service_controller_.AddAndFireAssistantClientObserver(&timer_controller_);

  conversation_controller_.AddActionObserver(&device_settings_controller_);
  conversation_controller_.AddActionObserver(&display_controller_);
  display_controller_.SetActionModule(conversation_controller_.action_module());
  platform_api_.SetAudioInputProvider(
      &audio_input_controller_.audio_input_provider());
}

LibassistantService::~LibassistantService() {
  // We explicitly stop the Libassistant service before destroying anything,
  // to prevent use-after-free bugs.
  service_controller_.Stop();
  service_controller_.RemoveAllAssistantClientObservers();
}

void LibassistantService::Bind(
    mojo::PendingReceiver<mojom::AudioInputController> audio_input_controller,
    mojo::PendingReceiver<mojom::ConversationController>
        conversation_controller,
    mojo::PendingReceiver<mojom::DisplayController> display_controller,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingReceiver<mojom::ServiceController> service_controller,
    mojo::PendingReceiver<mojom::SettingsController> settings_controller,
    mojo::PendingReceiver<mojom::SpeakerIdEnrollmentController>
        speaker_id_enrollment_controller,
    mojo::PendingReceiver<mojom::TimerController> timer_controller,
    mojo::PendingRemote<mojom::AudioOutputDelegate> audio_output_delegate,
    mojo::PendingRemote<mojom::DeviceSettingsDelegate> device_settings_delegate,
    mojo::PendingRemote<mojom::MediaDelegate> media_delegate,
    mojo::PendingRemote<mojom::NotificationDelegate> notification_delegate,
    mojo::PendingRemote<mojom::PlatformDelegate> platform_delegate,
    mojo::PendingRemote<mojom::TimerDelegate> timer_delegate) {
  platform_delegate_.Bind(std::move(platform_delegate));
  audio_input_controller_.Bind(std::move(audio_input_controller),
                               platform_delegate_.get());
  conversation_controller_.Bind(std::move(conversation_controller),
                                std::move(notification_delegate));
  device_settings_controller_.Bind(std::move(device_settings_delegate));
  display_controller_.Bind(std::move(display_controller));
  media_controller_.Bind(std::move(media_controller),
                         std::move(media_delegate));
  platform_api_.Bind(std::move(audio_output_delegate),
                     platform_delegate_.get());
  settings_controller_.Bind(std::move(settings_controller));
  service_controller_.Bind(std::move(service_controller),
                           &settings_controller_);
  speaker_id_enrollment_controller_.Bind(
      std::move(speaker_id_enrollment_controller));
  timer_controller_.Bind(std::move(timer_controller),
                         std::move(timer_delegate));
}

void LibassistantService::AddSpeechRecognitionObserver(
    mojo::PendingRemote<mojom::SpeechRecognitionObserver> observer) {
  speech_recognition_observers_.Add(std::move(observer));
}

void LibassistantService::AddAuthenticationStateObserver(
    mojo::PendingRemote<mojom::AuthenticationStateObserver> observer) {
  conversation_controller_.AddAuthenticationStateObserver(std::move(observer));
}

}  // namespace ash::libassistant
