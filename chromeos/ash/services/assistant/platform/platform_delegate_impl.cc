// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/platform/platform_delegate_impl.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"

namespace ash::assistant {

PlatformDelegateImpl::~PlatformDelegateImpl() = default;
PlatformDelegateImpl::PlatformDelegateImpl() = default;

void PlatformDelegateImpl::Bind(
    mojo::PendingReceiver<PlatformDelegate> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void PlatformDelegateImpl::Stop() {
  receiver_.reset();
}

void PlatformDelegateImpl::BindAudioStreamFactory(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
  AssistantBrowserDelegate::Get()->RequestAudioStreamFactory(
      std::move(receiver));
}

void PlatformDelegateImpl::BindAudioDecoderFactory(
    mojo::PendingReceiver<mojom::AssistantAudioDecoderFactory> receiver) {
  AssistantBrowserDelegate::Get()->RequestAudioDecoderFactory(
      std::move(receiver));
}

void PlatformDelegateImpl::BindBatteryMonitor(
    mojo::PendingReceiver<::device::mojom::BatteryMonitor> receiver) {
  AssistantBrowserDelegate::Get()->RequestBatteryMonitor(std::move(receiver));
}

void PlatformDelegateImpl::BindNetworkConfig(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  AssistantBrowserDelegate::Get()->RequestNetworkConfig(std::move(receiver));
}

void PlatformDelegateImpl::BindAssistantVolumeControl(
    mojo::PendingReceiver<::ash::mojom::AssistantVolumeControl> receiver) {
  AssistantBrowserDelegate::Get()->RequestAssistantVolumeControl(
      std::move(receiver));
}

void PlatformDelegateImpl::BindWakeLockProvider(
    mojo::PendingReceiver<::device::mojom::WakeLockProvider> receiver) {
  AssistantBrowserDelegate::Get()->RequestWakeLockProvider(std::move(receiver));
}

}  // namespace ash::assistant
