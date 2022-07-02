// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_PLATFORM_DELEGATE_IMPL_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_PLATFORM_DELEGATE_IMPL_H_

#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace assistant {

// Delegate that will fetch all instances from the |AssistantBrowserDelegate|.
class PlatformDelegateImpl
    : public chromeos::libassistant::mojom::PlatformDelegate {
 public:
  PlatformDelegateImpl();
  PlatformDelegateImpl(const PlatformDelegateImpl&) = delete;
  PlatformDelegateImpl& operator=(const PlatformDelegateImpl&) = delete;
  ~PlatformDelegateImpl() override;

  void Bind(mojo::PendingReceiver<PlatformDelegate> pending_receiver);

  // chromeos::libassistant::mojom::PlatformDelegate implementation:
  void BindAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver)
      override;
  void BindAudioDecoderFactory(
      mojo::PendingReceiver<
          chromeos::assistant::mojom::AssistantAudioDecoderFactory> receiver)
      override;
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

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_PLATFORM_DELEGATE_IMPL_H_
