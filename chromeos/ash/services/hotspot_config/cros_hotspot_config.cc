// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/hotspot_config/cros_hotspot_config.h"

#include "chromeos/ash/components/network/hotspot_controller.h"
#include "chromeos/ash/components/network/network_handler.h"

namespace ash::hotspot_config {

CrosHotspotConfig::CrosHotspotConfig()
    : CrosHotspotConfig(NetworkHandler::Get()->hotspot_capabilities_provider(),
                        NetworkHandler::Get()->hotspot_state_handler(),
                        NetworkHandler::Get()->hotspot_controller()) {}

CrosHotspotConfig::CrosHotspotConfig(
    ash::HotspotCapabilitiesProvider* hotspot_capabilities_provider,
    ash::HotspotStateHandler* hotspot_state_handler,
    ash::HotspotController* hotspot_controller)
    : hotspot_capabilities_provider_(hotspot_capabilities_provider),
      hotspot_state_handler_(hotspot_state_handler),
      hotspot_controller_(hotspot_controller) {}

CrosHotspotConfig::~CrosHotspotConfig() {
  if (hotspot_capabilities_provider_ &&
      hotspot_capabilities_provider_->HasObserver(this)) {
    hotspot_capabilities_provider_->RemoveObserver(this);
  }

  if (hotspot_state_handler_ && hotspot_state_handler_->HasObserver(this)) {
    hotspot_state_handler_->RemoveObserver(this);
  }
}

void CrosHotspotConfig::BindPendingReceiver(
    mojo::PendingReceiver<mojom::CrosHotspotConfig> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void CrosHotspotConfig::AddObserver(
    mojo::PendingRemote<mojom::CrosHotspotConfigObserver> observer) {
  if (hotspot_capabilities_provider_ &&
      !hotspot_capabilities_provider_->HasObserver(this)) {
    hotspot_capabilities_provider_->AddObserver(this);
  }
  if (hotspot_state_handler_ && !hotspot_state_handler_->HasObserver(this)) {
    hotspot_state_handler_->AddObserver(this);
  }

  observers_.Add(std::move(observer));
}

void CrosHotspotConfig::GetHotspotInfo(GetHotspotInfoCallback callback) {
  auto result = mojom::HotspotInfo::New();

  result->state = hotspot_state_handler_->GetHotspotState();
  result->client_count = hotspot_state_handler_->GetHotspotActiveClientCount();
  result->config = hotspot_state_handler_->GetHotspotConfig();
  result->allow_status =
      hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status;
  result->allowed_wifi_security_modes =
      hotspot_capabilities_provider_->GetHotspotCapabilities()
          .allowed_security_modes;

  std::move(callback).Run(std::move(result));
}

void CrosHotspotConfig::SetHotspotConfig(mojom::HotspotConfigPtr config,
                                         SetHotspotConfigCallback callback) {
  hotspot_state_handler_->SetHotspotConfig(std::move(config),
                                           std::move(callback));
}

void CrosHotspotConfig::EnableHotspot(EnableHotspotCallback callback) {
  hotspot_controller_->EnableHotspot(std::move(callback));
}

void CrosHotspotConfig::DisableHotspot(DisableHotspotCallback callback) {
  hotspot_controller_->DisableHotspot(std::move(callback));
}

// HotspotStateHandler::Observer:
void CrosHotspotConfig::OnHotspotStatusChanged() {
  for (auto& observer : observers_)
    observer->OnHotspotInfoChanged();
}

// HotspotCapabilitiesProvider::Observer:
void CrosHotspotConfig::OnHotspotCapabilitiesChanged() {
  for (auto& observer : observers_)
    observer->OnHotspotInfoChanged();
}

}  // namespace ash::hotspot_config
