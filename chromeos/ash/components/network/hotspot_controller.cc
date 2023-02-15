// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_controller.h"

#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/hotspot_util.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

HotspotController::HotspotControlRequest::HotspotControlRequest(
    bool enabled,
    HotspotControlCallback callback)
    : enabled(enabled), callback(std::move(callback)) {}

HotspotController::HotspotControlRequest::~HotspotControlRequest() = default;

HotspotController::HotspotController() = default;

HotspotController::~HotspotController() {
  if (technology_state_controller_) {
    technology_state_controller_->set_hotspot_operation_delegate(nullptr);
  }
}

void HotspotController::Init(
    HotspotCapabilitiesProvider* hotspot_capabilities_provider,
    HotspotStateHandler* hotspot_state_handler,
    TechnologyStateController* technology_state_controller) {
  hotspot_capabilities_provider_ = hotspot_capabilities_provider;
  hotspot_state_handler_ = hotspot_state_handler;
  technology_state_controller_ = technology_state_controller;
  technology_state_controller_->set_hotspot_operation_delegate(this);
}

void HotspotController::EnableHotspot(HotspotControlCallback callback) {
  queued_requests_.push(std::make_unique<HotspotControlRequest>(
      /*enabled=*/true, std::move(callback)));
  ProcessRequestQueue();
}

void HotspotController::DisableHotspot(HotspotControlCallback callback) {
  queued_requests_.push(std::make_unique<HotspotControlRequest>(
      /*enabled=*/false, std::move(callback)));
  ProcessRequestQueue();
}

void HotspotController::ProcessRequestQueue() {
  if (queued_requests_.empty())
    return;

  // A current request is already underway; wait until it has completed before
  // starting a new request.
  if (current_request_)
    return;

  current_request_ = std::move(queued_requests_.front());
  queued_requests_.pop();

  // Need to check the capabilities and do a final round of check tethering
  // readiness before enabling hotspot.
  if (current_request_->enabled) {
    CheckTetheringReadiness();
    return;
  }

  PerformSetTetheringEnabled(/*enabled=*/false);
}

void HotspotController::CheckTetheringReadiness() {
  if (hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status !=
      hotspot_config::mojom::HotspotAllowStatus::kAllowed) {
    CompleteCurrentRequest(
        hotspot_config::mojom::HotspotControlResult::kNotAllowed);
    return;
  }

  hotspot_capabilities_provider_->CheckTetheringReadiness(
      base::BindOnce(&HotspotController::OnCheckTetheringReadiness,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HotspotController::OnCheckTetheringReadiness(
    HotspotCapabilitiesProvider::CheckTetheringReadinessResult result) {
  if (result == HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
                    kUpstreamNetworkNotAvailable) {
    CompleteCurrentRequest(
        hotspot_config::mojom::HotspotControlResult::kUpstreamNotAvailable);
    return;
  }
  if (result !=
      HotspotCapabilitiesProvider::CheckTetheringReadinessResult::kReady) {
    CompleteCurrentRequest(
        hotspot_config::mojom::HotspotControlResult::kReadinessCheckFailed);
    return;
  }
  technology_state_controller_->PrepareEnableHotspot(
      base::BindOnce(&HotspotController::OnPrepareEnableHotspotCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HotspotController::OnPrepareEnableHotspotCompleted(bool prepare_success,
                                                        bool wifi_turned_off) {
  NET_LOG(EVENT) << "Prepare enable hotspot completed, success: "
                 << prepare_success << ", wifi turned off " << wifi_turned_off;
  current_request_->wifi_turned_off = wifi_turned_off;
  if (!prepare_success) {
    CompleteCurrentRequest(
        hotspot_config::mojom::HotspotControlResult::kDisableWifiFailed);
    return;
  }
  PerformSetTetheringEnabled(/*enabled=*/true);
}

void HotspotController::PerformSetTetheringEnabled(bool enabled) {
  ShillManagerClient::Get()->SetTetheringEnabled(
      enabled,
      base::BindOnce(&HotspotController::OnSetTetheringEnabledSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&HotspotController::OnSetTetheringEnabledFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HotspotController::OnSetTetheringEnabledSuccess(
    const std::string& result) {
  CompleteCurrentRequest(SetTetheringEnabledResultToMojom(result));
}

void HotspotController::OnSetTetheringEnabledFailure(
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Enable/disable tethering failed: " << error_name
                 << ", message: " << error_message;
  CompleteCurrentRequest(
      hotspot_config::mojom::HotspotControlResult::kShillOperationFailed);
}

void HotspotController::CompleteCurrentRequest(
    hotspot_config::mojom::HotspotControlResult result) {
  if (current_request_->wifi_turned_off && current_request_->enabled &&
      result != hotspot_config::mojom::HotspotControlResult::kSuccess) {
    // Turn Wifi back on if failed to enable hotspot.
    technology_state_controller_->SetTechnologiesEnabled(
        NetworkTypePattern::WiFi(), /*enabled=*/true,
        network_handler::ErrorCallback());
  }
  std::move(current_request_->callback).Run(result);
  current_request_.reset();

  ProcessRequestQueue();
}

void HotspotController::SetPolicyAllowHotspot(bool allow_hotspot) {
  if (allow_hotspot_ == allow_hotspot) {
    return;
  }

  hotspot_capabilities_provider_->SetPolicyAllowed(allow_hotspot);
  if (!allow_hotspot && hotspot_state_handler_->GetHotspotState() !=
                            hotspot_config::mojom::HotspotState::kDisabled) {
    DisableHotspot(base::DoNothing());
  }
}

void HotspotController::PrepareEnableWifi(
    base::OnceCallback<void(bool prepare_success)> callback) {
  if (hotspot_state_handler_->GetHotspotState() ==
          hotspot_config::mojom::HotspotState::kEnabled ||
      hotspot_state_handler_->GetHotspotState() ==
          hotspot_config::mojom::HotspotState::kEnabling) {
    DisableHotspot(
        base::BindOnce(&HotspotController::OnPrepareEnableWifiCompleted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  std::move(callback).Run(/*prepare_success=*/true);
}

void HotspotController::OnPrepareEnableWifiCompleted(
    base::OnceCallback<void(bool prepare_success)> callback,
    hotspot_config::mojom::HotspotControlResult control_result) {
  if (control_result == hotspot_config::mojom::HotspotControlResult::kSuccess) {
    std::move(callback).Run(/*prepare_success=*/true);
    return;
  }
  std::move(callback).Run(/*prepare_success=*/false);
}

}  //  namespace ash