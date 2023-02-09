// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_LIBASSISTANT_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_LIBASSISTANT_SERVICE_H_

#include "chromeos/ash/services/assistant/test_support/fake_service_controller.h"
#include "chromeos/ash/services/libassistant/public/mojom/notification_delegate.mojom-forward.h"
#include "chromeos/ash/services/libassistant/public/mojom/service.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::assistant {

// Fake implementation of the Libassistant Mojom service.
// It allows hooks to read and control the state of the service.
class FakeLibassistantService
    : public libassistant::mojom::LibassistantService {
 public:
  FakeLibassistantService();
  FakeLibassistantService(FakeLibassistantService&) = delete;
  FakeLibassistantService& operator=(FakeLibassistantService&) = delete;
  ~FakeLibassistantService() override;

  void Bind(mojo::PendingReceiver<libassistant::mojom::LibassistantService>
                pending_receiver);
  void Unbind();

  FakeServiceController& service_controller() { return service_controller_; }
  FakeServiceController& settings_controller() { return service_controller_; }

  // Return the receiver that was passed into the last Bind() call.
  mojo::PendingReceiver<libassistant::mojom::MediaController>
  GetMediaControllerPendingReceiver();
  mojo::PendingRemote<libassistant::mojom::MediaDelegate>
  GetMediaDelegatePendingRemote();
  mojo::PendingReceiver<libassistant::mojom::SpeakerIdEnrollmentController>
  GetSpeakerIdEnrollmentControllerPendingReceiver();

  // mojom::LibassistantService implementation:
  void Bind(
      mojo::PendingReceiver<libassistant::mojom::AudioInputController>
          audio_input_controller,
      mojo::PendingReceiver<libassistant::mojom::ConversationController>
          conversation_controller,
      mojo::PendingReceiver<libassistant::mojom::DisplayController>
          display_controller,
      mojo::PendingReceiver<libassistant::mojom::MediaController>
          media_controller,
      mojo::PendingReceiver<libassistant::mojom::ServiceController>
          service_controller,
      mojo::PendingReceiver<libassistant::mojom::SettingsController>
          settings_controller,
      mojo::PendingReceiver<libassistant::mojom::SpeakerIdEnrollmentController>
          speaker_id_enrollment_controller,
      mojo::PendingReceiver<libassistant::mojom::TimerController>
          timer_controller,
      mojo::PendingRemote<libassistant::mojom::AudioOutputDelegate>
          audio_output_delegate,
      mojo::PendingRemote<libassistant::mojom::DeviceSettingsDelegate>
          device_settings_delegate,
      mojo::PendingRemote<libassistant::mojom::MediaDelegate> media_delegate,
      mojo::PendingRemote<libassistant::mojom::NotificationDelegate>
          notification_delegate,
      mojo::PendingRemote<libassistant::mojom::PlatformDelegate>
          platform_delegate,
      mojo::PendingRemote<libassistant::mojom::TimerDelegate> timer_delegate)
      override;
  void AddSpeechRecognitionObserver(
      mojo::PendingRemote<libassistant::mojom::SpeechRecognitionObserver>
          observer) override {}
  void AddAuthenticationStateObserver(
      mojo::PendingRemote<libassistant::mojom::AuthenticationStateObserver>
          observer) override {}

  void FlushForTesting();

 private:
  mojo::Receiver<libassistant::mojom::LibassistantService> receiver_;

  mojo::PendingReceiver<libassistant::mojom::MediaController>
      media_controller_pending_receiver_;
  mojo::PendingReceiver<libassistant::mojom::SpeakerIdEnrollmentController>
      speaker_id_enrollment_controller_pending_receiver_;
  mojo::PendingRemote<libassistant::mojom::MediaDelegate>
      media_delegate_pending_remote_;

  FakeServiceController service_controller_;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_LIBASSISTANT_SERVICE_H_
