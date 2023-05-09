// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/ash/services/libassistant/audio_input_controller.h"
#include "chromeos/ash/services/libassistant/conversation_controller.h"
#include "chromeos/ash/services/libassistant/conversation_state_listener_impl.h"
#include "chromeos/ash/services/libassistant/device_settings_controller.h"
#include "chromeos/ash/services/libassistant/display_controller.h"
#include "chromeos/ash/services/libassistant/libassistant_factory.h"
#include "chromeos/ash/services/libassistant/media_controller.h"
#include "chromeos/ash/services/libassistant/platform_api.h"
#include "chromeos/ash/services/libassistant/public/mojom/notification_delegate.mojom-forward.h"
#include "chromeos/ash/services/libassistant/public/mojom/service.mojom.h"
#include "chromeos/ash/services/libassistant/service_controller.h"
#include "chromeos/ash/services/libassistant/settings_controller.h"
#include "chromeos/ash/services/libassistant/speaker_id_enrollment_controller.h"
#include "chromeos/ash/services/libassistant/timer_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::libassistant {

class COMPONENT_EXPORT(LIBASSISTANT_SERVICE) LibassistantService
    : public mojom::LibassistantService {
 public:
  explicit LibassistantService(
      mojo::PendingReceiver<mojom::LibassistantService> receiver,
      // Allows to inject a custom instance during unittests.
      std::unique_ptr<LibassistantFactory> factory = nullptr);
  LibassistantService(LibassistantService&) = delete;
  LibassistantService& operator=(LibassistantService&) = delete;
  ~LibassistantService() override;

  // mojom::LibassistantService implementation:
  void Bind(
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
      mojo::PendingRemote<mojom::DeviceSettingsDelegate>
          device_settings_delegate,
      mojo::PendingRemote<mojom::MediaDelegate> media_delegate,
      mojo::PendingRemote<mojom::NotificationDelegate> notification_delegate,
      mojo::PendingRemote<mojom::PlatformDelegate> platform_delegate,
      mojo::PendingRemote<mojom::TimerDelegate> timer_delegate) override;
  void AddSpeechRecognitionObserver(
      mojo::PendingRemote<mojom::SpeechRecognitionObserver> observer) override;
  void AddAuthenticationStateObserver(
      mojo::PendingRemote<mojom::AuthenticationStateObserver> observer)
      override;

  ConversationController& conversation_controller() {
    return conversation_controller_;
  }

  ServiceController& service_controller() { return service_controller_; }

  DisplayController& GetDisplayControllerForTesting() {
    return display_controller_;
  }

  SpeakerIdEnrollmentController&
  speaker_id_enrollment_controller_for_testing() {
    return speaker_id_enrollment_controller_;
  }

 private:
  mojo::Receiver<mojom::LibassistantService> receiver_;
  mojo::Remote<mojom::PlatformDelegate> platform_delegate_;

  mojo::RemoteSet<mojom::SpeechRecognitionObserver>
      speech_recognition_observers_;

  // These controllers are part of the platform api which is called from
  // Libassistant, and thus they must outlive |service_controller_|.
  PlatformApi platform_api_;
  AudioInputController audio_input_controller_;

  std::unique_ptr<LibassistantFactory> libassistant_factory_;
  ServiceController service_controller_;

  // These controllers call Libassistant, and thus they must *not* outlive
  // |service_controller_|.
  ConversationController conversation_controller_;
  ConversationStateListenerImpl conversation_state_listener_;
  DeviceSettingsController device_settings_controller_;
  DisplayController display_controller_;
  MediaController media_controller_;
  SettingsController settings_controller_;
  SpeakerIdEnrollmentController speaker_id_enrollment_controller_;
  TimerController timer_controller_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_LIBASSISTANT_SERVICE_H_
