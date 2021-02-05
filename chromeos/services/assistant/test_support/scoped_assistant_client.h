// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_SCOPED_ASSISTANT_CLIENT_H_
#define CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_SCOPED_ASSISTANT_CLIENT_H_

#include "base/macros.h"
#include "chromeos/services/assistant/public/cpp/assistant_client.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace assistant {

// A base testing implementation of the AssistantClient interface which tests
// can subclass to implement specific client mocking support. It also installs
// itself as the singleton instance.
class ScopedAssistantClient : AssistantClient {
 public:
  ScopedAssistantClient();
  ~ScopedAssistantClient() override;

  AssistantClient& Get();

  // Set the MediaControllerManager receiver that will be bound to the remote
  // passed into RequestMediaControllerManager().
  void SetMediaControllerManager(
      mojo::Receiver<media_session::mojom::MediaControllerManager>* receiver);

  // AssistantClient implementation:
  void OnAssistantStatusChanged(AssistantStatus status) override {}
  void RequestAssistantVolumeControl(
      mojo::PendingReceiver<ash::mojom::AssistantVolumeControl> receiver)
      override {}
  void RequestBatteryMonitor(
      mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) override {}
  void RequestWakeLockProvider(
      mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver)
      override {}
  void RequestAudioStreamFactory(
      mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) override {}
  void RequestAudioDecoderFactory(
      mojo::PendingReceiver<mojom::AssistantAudioDecoderFactory> receiver)
      override {}
  void RequestAudioFocusManager(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override {}
  void RequestMediaControllerManager(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override;
  void RequestNetworkConfig(
      mojo::PendingReceiver<network_config::mojom::CrosNetworkConfig> receiver)
      override {}

 private:
  mojo::Receiver<media_session::mojom::MediaControllerManager>*
      media_controller_manager_receiver_ = nullptr;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_TEST_SUPPORT_SCOPED_ASSISTANT_CLIENT_H_
