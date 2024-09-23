// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/technology_state_controller.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/network/metrics/network_metrics_helper.h"
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

void TechnologyStateController::PrepareEnableHotspot(
    PrepareEnableHotspotCallback callback) {
  NetworkStateHandler::TechnologyState wifi_state =
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi());
  if (wifi_state == NetworkStateHandler::TECHNOLOGY_ENABLED) {
    auto callback_split = base::SplitOnceCallback(std::move(callback));
    network_state_handler_->SetTechnologyEnabled(
        NetworkTypePattern::WiFi(), /*enabled=*/false,
        base::BindOnce(std::move(callback_split.first),
                       /*prepare_success=*/true, /*wifi_turned_off=*/true),
        base::BindOnce(
            &TechnologyStateController::OnDisableWifiForHotspotFailed,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback_split.second)));
    return;
  }

  // Wifi state shouldn't be 'enabling' when attempting to turn on hotspot, as
  // the UI should prevent such actions.
  if (wifi_state == NetworkStateHandler::TECHNOLOGY_ENABLING) {
    NET_LOG(ERROR) << "Wifi technology is enabling when attempting to turn on "
                   << "hotspot.";
    std::move(callback).Run(/*prepare_success=*/false,
                            /*wifi_turned_off=*/false);
    return;
  }

  std::move(callback).Run(/*prepare_success=*/true, /*wifi_turned_off=*/false);
}

void TechnologyStateController::OnDisableWifiForHotspotFailed(
    PrepareEnableHotspotCallback callback,
    const std::string& error_name) {
  NET_LOG(ERROR) << "Failed to disable Wifi during hotspot enable preparation, "
                 << "error: " << error_name;
  std::move(callback).Run(/*prepare_success=*/false, /*wifi_turned_off=*/false);
}

void TechnologyStateController::SetTechnologiesEnabled(
    const NetworkTypePattern& type,
    bool enabled,
    network_handler::ErrorCallback error_callback) {
  if (!hotspot_operation_delegate_) {
    NET_LOG(ERROR) << "hotspot operation delegate is null while hotspot flag is"
                   << " on.";
    network_handler::RunErrorCallback(std::move(error_callback),
                                      kErrorDisableHotspot);
    return;
  }

  if (enabled && type.MatchesPattern(NetworkTypePattern::WiFi())) {
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
  if (success) {
    network_state_handler_->SetTechnologiesEnabled(type, /*enabled=*/true,
                                                   std::move(error_callback));
    return;
  }
  NetworkMetricsHelper::LogEnableTechnologyResult(
      shill::kTypeWifi, /*success=*/false, kErrorDisableHotspot);
  network_handler::RunErrorCallback(std::move(error_callback),
                                    kErrorDisableHotspot);
}

}  // namespace ash