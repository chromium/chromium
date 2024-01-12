// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_TEST_SUPPORT_LIBASSISTANT_SERVICE_TESTER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_TEST_SUPPORT_LIBASSISTANT_SERVICE_TESTER_H_

#include "base/memory/raw_ptr.h"
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
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::libassistant {

class AssistantClient;
class DisplayConnection;
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

  mojom::AudioInputController& audio_input_controller() {
    return *audio_input_controller_.get();
  }
  mojom::ConversationController& conversation_controller() {
    return *conversation_controller_.get();
  }
  mojom::DisplayController& display_controller() {
    return *display_controller_.get();
  }
  mojom::ServiceController& service_controller() {
    return *service_controller_.get();
  }
  mojom::SpeakerIdEnrollmentController& speaker_id_enrollment_controller() {
    return *speaker_id_enrollment_controller_.get();
  }

  mojo::PendingReceiver<mojom::NotificationDelegate>
  GetNotificationDelegatePendingReceiver();

  DisplayConnection& GetDisplayConnection();

  void FlushForTesting();

 private:
  void BindControllers();

  mojo::Remote<mojom::AudioInputController> audio_input_controller_;
  mojo::Remote<mojom::ConversationController> conversation_controller_;
  mojo::Remote<mojom::DisplayController> display_controller_;
  mojo::Remote<mojom::MediaController> media_controller_;
  mojo::Remote<mojom::ServiceController> service_controller_;
  mojo::Remote<mojom::SettingsController> settings_controller_;
  mojo::Remote<mojom::SpeakerIdEnrollmentController>
      speaker_id_enrollment_controller_;
  mojo::Remote<mojom::TimerController> timer_controller_;
  mojo::PendingReceiver<mojom::AudioOutputDelegate>
      pending_audio_output_delegate_;
  mojo::PendingReceiver<mojom::DeviceSettingsDelegate>
      pending_device_settings_delegate_;
  mojo::PendingReceiver<mojom::MediaDelegate> pending_media_delegate_;
  mojo::PendingReceiver<mojom::NotificationDelegate>
      pending_notification_delegate_;
  mojo::PendingReceiver<mojom::PlatformDelegate> pending_platform_delegate_;
  mojo::PendingReceiver<mojom::TimerDelegate> pending_timer_delegate_;

  mojo::Remote<mojom::LibassistantService> service_remote_;
  // Our file provider requires the home dir to be overridden.
  base::ScopedPathOverride home_dir_override_;
  raw_ptr<FakeLibassistantFactory> libassistant_factory_ = nullptr;
  std::unique_ptr<LibassistantService> service_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_TEST_SUPPORT_LIBASSISTANT_SERVICE_TESTER_H_
