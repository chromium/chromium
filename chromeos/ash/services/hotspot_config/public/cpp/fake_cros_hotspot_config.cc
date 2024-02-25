// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/hotspot_config/public/cpp/fake_cros_hotspot_config.h"

#include "chromeos/ash/services/hotspot_config/cros_hotspot_config.h"

namespace ash::hotspot_config {

FakeCrosHotspotConfig::FakeCrosHotspotConfig() {
  hotspot_info_ = mojom::HotspotInfo::New();
}

FakeCrosHotspotConfig::~FakeCrosHotspotConfig() = default;

void FakeCrosHotspotConfig::ObserveEnabledStateChanges(
    mojo::PendingRemote<mojom::HotspotEnabledStateObserver> observer) {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler* network_handler = NetworkHandler::Get();
    network_handler->hotspot_enabled_state_notifier()
        ->ObserveEnabledStateChanges(std::move(observer));
  } else {
    hotspot_enabled_state_observers_.Add(std::move(observer));
  }
}

void FakeCrosHotspotConfig::AddObserver(
    mojo::PendingRemote<mojom::CrosHotspotConfigObserver> observer) {
  auto observer_id = cros_hotspot_config_observers_.Add(std::move(observer));
  cros_hotspot_config_observers_.Get(observer_id)->OnHotspotInfoChanged();
}

void FakeCrosHotspotConfig::GetHotspotInfo(GetHotspotInfoCallback callback) {
  std::move(callback).Run(mojo::Clone(hotspot_info_));
}

void FakeCrosHotspotConfig::SetHotspotConfig(
    mojom::HotspotConfigPtr config,
    SetHotspotConfigCallback callback) {
  hotspot_info_->config = mojo::Clone(config);
  NotifyHotspotInfoObservers();
}

void FakeCrosHotspotConfig::EnableHotspot(EnableHotspotCallback callback) {
  hotspot_info_->state = mojom::HotspotState::kEnabled;
  NotifyHotspotInfoObservers();
  NotifyHotspotTurnedOn();
  std::move(callback).Run(mojom::HotspotControlResult::kSuccess);
}

void FakeCrosHotspotConfig::DisableHotspot(DisableHotspotCallback callback) {
  hotspot_info_->state = mojom::HotspotState::kDisabled;
  NotifyHotspotInfoObservers();
  NotifyHotspotTurnedOff(mojom::DisableReason::kUserInitiated);
  std::move(callback).Run(mojom::HotspotControlResult::kSuccess);
}

mojo::PendingRemote<mojom::CrosHotspotConfig>
FakeCrosHotspotConfig::GetPendingRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeCrosHotspotConfig::SetFakeHotspotInfo(
    mojom::HotspotInfoPtr hotspot_info) {
  hotspot_info_ = mojo::Clone(hotspot_info);
  NotifyHotspotInfoObservers();
}

void FakeCrosHotspotConfig::NotifyHotspotInfoObservers() {
  for (auto& observer : cros_hotspot_config_observers_) {
    observer->OnHotspotInfoChanged();
  }
}

void FakeCrosHotspotConfig::NotifyHotspotTurnedOn() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler* network_handler = NetworkHandler::Get();
    network_handler->hotspot_enabled_state_notifier()->OnHotspotTurnedOn();
  }

  for (auto& observer : hotspot_enabled_state_observers_) {
    observer->OnHotspotTurnedOn();
  }
}

void FakeCrosHotspotConfig::NotifyHotspotTurnedOff(
    mojom::DisableReason reason) {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler* network_handler = NetworkHandler::Get();
    network_handler->hotspot_enabled_state_notifier()->OnHotspotTurnedOff(
        reason);
  }

  for (auto& observer : hotspot_enabled_state_observers_) {
    observer->OnHotspotTurnedOff(reason);
  }
}

}  // namespace ash::hotspot_config
