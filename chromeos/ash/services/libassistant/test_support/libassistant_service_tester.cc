// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/test_support/libassistant_service_tester.h"

#include "base/base_paths.h"
#include "chromeos/ash/services/libassistant/display_connection.h"
#include "chromeos/ash/services/libassistant/display_controller.h"
#include "chromeos/ash/services/libassistant/grpc/services_status_observer.h"
#include "chromeos/ash/services/libassistant/public/mojom/notification_delegate.mojom-forward.h"
#include "chromeos/ash/services/libassistant/service_controller.h"
#include "chromeos/ash/services/libassistant/test_support/fake_libassistant_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace ash::libassistant {

namespace {

mojo::PendingRemote<network::mojom::URLLoaderFactory> BindURLLoaderFactory() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> result;
  network::TestURLLoaderFactory().Clone(
      result.InitWithNewPipeAndPassReceiver());
  return result;
}

}  // namespace

LibassistantServiceTester::LibassistantServiceTester()
    : home_dir_override_(base::DIR_HOME) {
  auto factory = std::make_unique<FakeLibassistantFactory>();
  // Keep a pointer around.
  libassistant_factory_ = factory.get();

  service_ = std::make_unique<LibassistantService>(
      service_remote_.BindNewPipeAndPassReceiver(), std::move(factory));

  BindControllers();
}

LibassistantServiceTester::~LibassistantServiceTester() = default;

AssistantClient& LibassistantServiceTester::assistant_client() {
  return *(service_->service_controller().assistant_client());
}

chromeos::assistant::FakeAssistantManager&
LibassistantServiceTester::assistant_manager() {
  return libassistant_factory_->assistant_manager();
}

void LibassistantServiceTester::Start() {
  service_controller_->Initialize(mojom::BootupConfig::New(),
                                  BindURLLoaderFactory());
  service_controller_->Start();
  service_controller_.FlushForTesting();
  // Simulate gRPC heartbeat of the booting up signal.
  service_->service_controller().OnServicesStatusChanged(
      ServicesStatus::ONLINE_BOOTING_UP);
}

void LibassistantServiceTester::BindControllers() {
  mojo::PendingRemote<mojom::AudioOutputDelegate>
      pending_audio_output_delegate_remote;
  mojo::PendingRemote<mojom::DeviceSettingsDelegate>
      pending_device_settings_delegate_remote;
  mojo::PendingRemote<mojom::MediaDelegate> pending_media_delegate_remote;
  mojo::PendingRemote<mojom::NotificationDelegate>
      pending_notification_delegate_remote;
  mojo::PendingRemote<mojom::PlatformDelegate> pending_platform_delegate_remote;
  mojo::PendingRemote<mojom::TimerDelegate> pending_timer_delegate_remote;

  pending_audio_output_delegate_ =
      pending_audio_output_delegate_remote.InitWithNewPipeAndPassReceiver();
  pending_device_settings_delegate_ =
      pending_device_settings_delegate_remote.InitWithNewPipeAndPassReceiver();
  pending_media_delegate_ =
      pending_media_delegate_remote.InitWithNewPipeAndPassReceiver();
  pending_notification_delegate_ =
      pending_notification_delegate_remote.InitWithNewPipeAndPassReceiver();
  pending_platform_delegate_ =
      pending_platform_delegate_remote.InitWithNewPipeAndPassReceiver();
  pending_timer_delegate_ =
      pending_timer_delegate_remote.InitWithNewPipeAndPassReceiver();

  service_->Bind(audio_input_controller_.BindNewPipeAndPassReceiver(),
                 conversation_controller_.BindNewPipeAndPassReceiver(),
                 display_controller_.BindNewPipeAndPassReceiver(),
                 media_controller_.BindNewPipeAndPassReceiver(),
                 service_controller_.BindNewPipeAndPassReceiver(),
                 settings_controller_.BindNewPipeAndPassReceiver(),
                 speaker_id_enrollment_controller_.BindNewPipeAndPassReceiver(),
                 timer_controller_.BindNewPipeAndPassReceiver(),
                 std::move(pending_audio_output_delegate_remote),
                 std::move(pending_device_settings_delegate_remote),
                 std::move(pending_media_delegate_remote),
                 std::move(pending_notification_delegate_remote),
                 std::move(pending_platform_delegate_remote),
                 std::move(pending_timer_delegate_remote));
}

mojo::PendingReceiver<mojom::NotificationDelegate>
LibassistantServiceTester::GetNotificationDelegatePendingReceiver() {
  DCHECK(pending_notification_delegate_.is_valid());
  return std::move(pending_notification_delegate_);
}

DisplayConnection& LibassistantServiceTester::GetDisplayConnection() {
  return service_->GetDisplayControllerForTesting()
      .GetDisplayConnectionForTesting();
}

void LibassistantServiceTester::FlushForTesting() {
  audio_input_controller_.FlushForTesting();
  conversation_controller_.FlushForTesting();
  display_controller_.FlushForTesting();
  media_controller_.FlushForTesting();
  service_controller_.FlushForTesting();
  speaker_id_enrollment_controller_.FlushForTesting();
  service_remote_.FlushForTesting();
  timer_controller_.FlushForTesting();
}

}  // namespace ash::libassistant
