// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_TEST_SUPPORT_LIBASSISTANT_SERVICE_TESTER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_TEST_SUPPORT_LIBASSISTANT_SERVICE_TESTER_H_

#include "base/test/scoped_path_override.h"
#include "chromeos/ash/services/libassistant/libassistant_service.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_output_delegate.mojom-forward.h"
#include "chromeos/ash/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/device_settings_delegate.mojom-forward.h"
#include "chromeos/ash/services/libassistant/public/mojom/display_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/notification_delegate.mojom-forward.h"
#include "chromeos/ash/services/libassistant/public/mojom/service.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/service_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom-forward.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::libassistant {

class AssistantClient;
class FakeLibassistantFactory;

// Helper class that makes it easier to test |LibassistantService|.
class LibassistantServiceTester {
 public:
  LibassistantServiceTester();
  LibassistantServiceTester(const LibassistantServiceTester&) = delete;
  LibassistantServiceTester& operator=(const LibassistantServiceTester&) =
      delete;
  ~LibassistantServiceTester();

  // Initialize and start Libassistant.
  void Start();

  LibassistantService& service() { return *service_; }

  AssistantClient& assistant_client();
  chromeos::assistant::FakeAssistantManager& assistant_manager();
  chromeos::assistant::FakeAssistantManagerInternal&
  assistant_manager_internal();

  chromeos::libassistant::mojom::AudioInputController&
  audio_input_controller() {
    return *audio_input_controller_.get();
  }
  chromeos::libassistant::mojom::ConversationController&
  conversation_controller() {
    return *conversation_controller_.get();
  }
  chromeos::libassistant::mojom::DisplayController& display_controller() {
    return *display_controller_.get();
  }
  chromeos::libassistant::mojom::ServiceController& service_controller() {
    return *service_controller_.get();
  }
  chromeos::libassistant::mojom::SpeakerIdEnrollmentController&
  speaker_id_enrollment_controller() {
    return *speaker_id_enrollment_controller_.get();
  }

  mojo::PendingReceiver<chromeos::libassistant::mojom::NotificationDelegate>
  GetNotificationDelegatePendingReceiver();

  void FlushForTesting();

 private:
  void BindControllers();

  mojo::Remote<chromeos::libassistant::mojom::AudioInputController>
      audio_input_controller_;
  mojo::Remote<chromeos::libassistant::mojom::ConversationController>
      conversation_controller_;
  mojo::Remote<chromeos::libassistant::mojom::DisplayController>
      display_controller_;
  mojo::Remote<chromeos::libassistant::mojom::MediaController>
      media_controller_;
  mojo::Remote<chromeos::libassistant::mojom::ServiceController>
      service_controller_;
  mojo::Remote<chromeos::libassistant::mojom::SettingsController>
      settings_controller_;
  mojo::Remote<chromeos::libassistant::mojom::SpeakerIdEnrollmentController>
      speaker_id_enrollment_controller_;
  mojo::Remote<chromeos::libassistant::mojom::TimerController>
      timer_controller_;
  mojo::PendingReceiver<chromeos::libassistant::mojom::AudioOutputDelegate>
      pending_audio_output_delegate_;
  mojo::PendingReceiver<chromeos::libassistant::mojom::DeviceSettingsDelegate>
      pending_device_settings_delegate_;
  mojo::PendingReceiver<chromeos::libassistant::mojom::MediaDelegate>
      pending_media_delegate_;
  mojo::PendingReceiver<chromeos::libassistant::mojom::NotificationDelegate>
      pending_notification_delegate_;
  mojo::PendingReceiver<chromeos::libassistant::mojom::PlatformDelegate>
      pending_platform_delegate_;
  mojo::PendingReceiver<chromeos::libassistant::mojom::TimerDelegate>
      pending_timer_delegate_;

  mojo::Remote<chromeos::libassistant::mojom::LibassistantService>
      service_remote_;
  // Our file provider requires the home dir to be overridden.
  base::ScopedPathOverride home_dir_override_;
  FakeLibassistantFactory* libassistant_factory_ = nullptr;
  std::unique_ptr<LibassistantService> service_;
};

}  // namespace ash::libassistant

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos::libassistant {
using ::ash::libassistant::LibassistantServiceTester;
}

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_TEST_SUPPORT_LIBASSISTANT_SERVICE_TESTER_H_
