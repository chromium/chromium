// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_SCOPED_ASSISTANT_BROWSER_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_SCOPED_ASSISTANT_BROWSER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::assistant {

// A base testing implementation of the AssistantBrowserDelegate interface which
// tests can subclass to implement specific client mocking support. It also
// installs itself as the singleton instance.
class ScopedAssistantBrowserDelegate : AssistantBrowserDelegate {
 public:
  ScopedAssistantBrowserDelegate();
  ~ScopedAssistantBrowserDelegate() override;

  AssistantBrowserDelegate& Get();

  // Set the MediaControllerManager receiver that will be bound to the remote
  // passed into RequestMediaControllerManager().
  void SetMediaControllerManager(
      mojo::Receiver<media_session::mojom::MediaControllerManager>* receiver);

  // AssistantBrowserDelegate implementation:
  void OnAssistantStatusChanged(AssistantStatus status) override {}
  void RequestAssistantVolumeControl(
      mojo::PendingReceiver<::ash::mojom::AssistantVolumeControl> receiver)
      override {}
  void RequestBatteryMonitor(
      mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) override {}
  void RequestWakeLockProvider(
      mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver)
      override {}
  void RequestAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver)
      override {}
  void RequestAudioDecoderFactory(
      mojo::PendingReceiver<assistant::mojom::AssistantAudioDecoderFactory>
          receiver) override {}
  void RequestAudioFocusManager(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override {}
  void RequestMediaControllerManager(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override;
  void RequestNetworkConfig(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver) override {}
  void OpenUrl(GURL url) override;
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  void RequestLibassistantService(
      mojo::PendingReceiver<libassistant::mojom::LibassistantService> receiver)
      override {}
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)

 private:
  raw_ptr<mojo::Receiver<media_session::mojom::MediaControllerManager>>
      media_controller_manager_receiver_ = nullptr;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_SCOPED_ASSISTANT_BROWSER_DELEGATE_H_
