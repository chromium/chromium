// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_PLATFORM_DELEGATE_IMPL_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_PLATFORM_DELEGATE_IMPL_H_

#include "chromeos/ash/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::assistant {

// Delegate that will fetch all instances from the |AssistantBrowserDelegate|.
class PlatformDelegateImpl : public libassistant::mojom::PlatformDelegate {
 public:
  PlatformDelegateImpl();
  PlatformDelegateImpl(const PlatformDelegateImpl&) = delete;
  PlatformDelegateImpl& operator=(const PlatformDelegateImpl&) = delete;
  ~PlatformDelegateImpl() override;

  void Bind(mojo::PendingReceiver<PlatformDelegate> pending_receiver);
  void Stop();

  // libassistant::mojom::PlatformDelegate implementation:
  void BindAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver)
      override;
  void BindAudioDecoderFactory(
      mojo::PendingReceiver<assistant::mojom::AssistantAudioDecoderFactory>
          receiver) override;
  void BindBatteryMonitor(
      mojo::PendingReceiver<::device::mojom::BatteryMonitor> receiver) override;
  void BindNetworkConfig(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver) override;
  void BindAssistantVolumeControl(
      mojo::PendingReceiver<::ash::mojom::AssistantVolumeControl> receiver)
      override;
  void BindWakeLockProvider(
      mojo::PendingReceiver<::device::mojom::WakeLockProvider> receiver)
      override;

 private:
  mojo::Receiver<PlatformDelegate> receiver_{this};
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_PLATFORM_DELEGATE_IMPL_H_
