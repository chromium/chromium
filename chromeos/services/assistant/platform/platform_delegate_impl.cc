// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/platform_delegate_impl.h"
#include "chromeos/services/assistant/public/cpp/assistant_client.h"

namespace chromeos {
namespace assistant {

PlatformDelegateImpl::~PlatformDelegateImpl() = default;
PlatformDelegateImpl::PlatformDelegateImpl() = default;

void PlatformDelegateImpl::Bind(
    mojo::PendingReceiver<PlatformDelegate> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void PlatformDelegateImpl::BindAudioStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  AssistantClient::Get()->RequestAudioStreamFactory(std::move(receiver));
}

void PlatformDelegateImpl::BindAudioDecoderFactory(
    mojo::PendingReceiver<
        chromeos::assistant::mojom::AssistantAudioDecoderFactory> receiver) {
  AssistantClient::Get()->RequestAudioDecoderFactory(std::move(receiver));
}

void PlatformDelegateImpl::BindBatteryMonitor(
    mojo::PendingReceiver<::device::mojom::BatteryMonitor> receiver) {
  AssistantClient::Get()->RequestBatteryMonitor(std::move(receiver));
}

void PlatformDelegateImpl::BindNetworkConfig(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  AssistantClient::Get()->RequestNetworkConfig(std::move(receiver));
}

void PlatformDelegateImpl::BindAssistantVolumeControl(
    mojo::PendingReceiver<::ash::mojom::AssistantVolumeControl> receiver) {
  AssistantClient::Get()->RequestAssistantVolumeControl(std::move(receiver));
}

void PlatformDelegateImpl::BindWakeLockProvider(
    mojo::PendingReceiver<::device::mojom::WakeLockProvider> receiver) {
  AssistantClient::Get()->RequestWakeLockProvider(std::move(receiver));
}

}  // namespace assistant
}  // namespace chromeos
