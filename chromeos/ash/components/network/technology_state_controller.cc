// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/technology_state_controller.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

// static
const char TechnologyStateController::kErrorDisableHotspot[] =
    "disable-hotspot-failed";

TechnologyStateController::TechnologyStateController() = default;

TechnologyStateController::~TechnologyStateController() = default;

void TechnologyStateController::Init(
    NetworkStateHandler* network_state_handler) {
  network_state_handler_ = network_state_handler;
}

void TechnologyStateController::SetTechnologiesEnabled(
    const NetworkTypePattern& type,
    bool enabled,
    network_handler::ErrorCallback error_callback) {
  if (!hotspot_operation_delegate_ && ash::features::IsHotspotEnabled()) {
    NET_LOG(ERROR) << "hotspot operation delegate is null while hotspot flag is"
                   << " on.";
    network_handler::RunErrorCallback(std::move(error_callback),
                                      kErrorDisableHotspot);
    return;
  }

  if (ash::features::IsHotspotEnabled() && enabled &&
      type.MatchesPattern(NetworkTypePattern::WiFi())) {
    hotspot_operation_delegate_->PrepareEnableWifi(base::BindOnce(
        &TechnologyStateController::OnPrepareEnableWifiCompleted,
        weak_ptr_factory_.GetWeakPtr(), type, std::move(error_callback)));
    return;
  }

  network_state_handler_->SetTechnologiesEnabled(type, enabled,
                                                 std::move(error_callback));
}

void TechnologyStateController::OnPrepareEnableWifiCompleted(
    const NetworkTypePattern& type,
    network_handler::ErrorCallback error_callback,
    bool success) {
  DCHECK(ash::features::IsHotspotEnabled());

  if (success) {
    network_state_handler_->SetTechnologiesEnabled(type, /*enabled=*/true,
                                                   std::move(error_callback));
    return;
  }
  network_handler::RunErrorCallback(std::move(error_callback),
                                    kErrorDisableHotspot);
}

}  // namespace ash