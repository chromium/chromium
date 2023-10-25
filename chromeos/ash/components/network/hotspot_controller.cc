// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_controller.h"

#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/hotspot_util.h"
#include "chromeos/ash/components/network/metrics/hotspot_feature_usage_metrics.h"
#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

HotspotController::HotspotControlRequest::HotspotControlRequest(
    bool enabled,
    absl::optional<hotspot_config::mojom::DisableReason> disable_reason,
    HotspotControlCallback callback)
    : enabled(enabled),
      disable_reason(disable_reason),
      callback(std::move(callback)) {}

HotspotController::HotspotControlRequest::~HotspotControlRequest() = default;

HotspotController::HotspotController() = default;

HotspotController::~HotspotController() {
  if (technology_state_controller_) {
    technology_state_controller_->set_hotspot_operation_delegate(nullptr);
  }
  if (hotspot_state_handler_ && hotspot_state_handler_->HasObserver(this)) {
    hotspot_state_handler_->RemoveObserver(this);
  }
}

void HotspotController::Init(
    HotspotCapabilitiesProvider* hotspot_capabilities_provider,
    HotspotFeatureUsageMetrics* hotspot_feature_usage_metrics,
    HotspotStateHandler* hotspot_state_handler,
    TechnologyStateController* technology_state_controller) {
  hotspot_capabilities_provider_ = hotspot_capabilities_provider;
  hotspot_feature_usage_metrics_ = hotspot_feature_usage_metrics;
  hotspot_state_handler_ = hotspot_state_handler;
  hotspot_state_handler_->AddObserver(this);
  technology_state_controller_ = technology_state_controller;
  technology_state_controller_->set_hotspot_operation_delegate(this);
}

void HotspotController::EnableHotspot(HotspotControlCallback callback) {
  if (current_disable_request_ &&
      current_disable_request_->disable_reason !=
          hotspot_config::mojom::DisableReason::kRestart) {
    NET_LOG(ERROR) << "Failed to enable hotspot as a non-restart disable "
                      "request is in progress";
    HotspotMetricsHelper::RecordSetTetheringEnabledResult(
        /*enabled=*/true,
        hotspot_config::mojom::HotspotControlResult::kInvalid);
    return;
  }
  if (!current_enable_request_) {
    current_enable_request_ = std::make_unique<HotspotControlRequest>(
        /*enabled=*/true, /*disable_reason=*/absl::nullopt,
        std::move(callback));
    if (hotspot_state_handler_->GetHotspotState() ==
        hotspot_config::mojom::HotspotState::kEnabled) {
      CompleteEnableRequest(
          hotspot_config::mojom::HotspotControlResult::kAlreadyFulfilled);
      return;
    }
    current_enable_request_->enable_latency_timer = base::ElapsedTimer();
    CheckTetheringReadiness();
  }
}

void HotspotController::DisableHotspot(
    HotspotControlCallback callback,
    hotspot_config::mojom::DisableReason disable_reason) {
  if (current_enable_request_) {
    current_enable_request_->abort = true;
    if (hotspot_state_handler_->GetHotspotState() ==
        hotspot_config::mojom::HotspotState::kEnabling) {
      current_disable_request_ = std::make_unique<HotspotControlRequest>(
          /*enabled=*/false, disable_reason, std::move(callback));
      PerformSetTetheringEnabled(/*enabled=*/false);
    }
    return;
  }
  if (!current_disable_request_) {
    current_disable_request_ = std::make_unique<HotspotControlRequest>(
        /*enabled=*/false, disable_reason, std::move(callback));
    if (hotspot_state_handler_->GetHotspotState() ==
        hotspot_config::mojom::HotspotState::kDisabled) {
      CompleteDisableRequest(
          hotspot_config::mojom::HotspotControlResult::kAlreadyFulfilled);
      return;
    }
    PerformSetTetheringEnabled(/*enabled=*/false);
  }
}

void HotspotController::RestartHotspotIfActive() {
  DisableHotspot(
      base::BindOnce(&HotspotController::OnDisableHotspotCompleteForRestart,
                     weak_ptr_factory_.GetWeakPtr()),
      hotspot_config::mojom::DisableReason::kRestart);
}

void HotspotController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HotspotController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool HotspotController::HasObserver(Observer* observer) const {
  return observer_list_.HasObserver(observer);
}

void HotspotController::CheckTetheringReadiness() {
  if (hotspot_capabilities_provider_->GetHotspotCapabilities().allow_status !=
      hotspot_config::mojom::HotspotAllowStatus::kAllowed) {
    CompleteEnableRequest(
        hotspot_config::mojom::HotspotControlResult::kNotAllowed);
    return;
  }

  hotspot_capabilities_provider_->CheckTetheringReadiness(
      base::BindOnce(&HotspotController::OnCheckTetheringReadiness,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HotspotController::OnCheckTetheringReadiness(
    HotspotCapabilitiesProvider::CheckTetheringReadinessResult result) {
  if (current_enable_request_->abort) {
    NET_LOG(ERROR) << "Aborting in check tethering readiness";
    CompleteEnableRequest(
        hotspot_config::mojom::HotspotControlResult::kAborted);
    return;
  }
  if (result == HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
                    kUpstreamNetworkNotAvailable) {
    CompleteEnableRequest(
        hotspot_config::mojom::HotspotControlResult::kUpstreamNotAvailable);
    return;
  }
  if (result !=
      HotspotCapabilitiesProvider::CheckTetheringReadinessResult::kReady) {
    CompleteEnableRequest(
        hotspot_config::mojom::HotspotControlResult::kReadinessCheckFailed);
    return;
  }
  technology_state_controller_->PrepareEnableHotspot(
      base::BindOnce(&HotspotController::OnPrepareEnableHotspotCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HotspotController::OnPrepareEnableHotspotCompleted(bool prepare_success,
                                                        bool wifi_turned_off) {
  if (current_enable_request_->abort) {
    CompleteEnableRequest(
        hotspot_config::mojom::HotspotControlResult::kAborted);
    return;
  }
  NET_LOG(EVENT) << "Prepare enable hotspot completed, success: "
                 << prepare_success << ", wifi turned off " << wifi_turned_off;
  wifi_turned_off_ = wifi_turned_off;
  if (!prepare_success) {
    CompleteEnableRequest(
        hotspot_config::mojom::HotspotControlResult::kDisableWifiFailed);
    return;
  }
  PerformSetTetheringEnabled(/*enabled=*/true);
}

void HotspotController::PerformSetTetheringEnabled(bool enabled) {
  if (enabled && current_enable_request_->abort) {
    CompleteEnableRequest(
        hotspot_config::mojom::HotspotControlResult::kAborted);
    return;
  }
  ShillManagerClient::Get()->SetTetheringEnabled(
      enabled,
      base::BindOnce(&HotspotController::OnSetTetheringEnabledSuccess,
                     weak_ptr_factory_.GetWeakPtr(), enabled),
      base::BindOnce(&HotspotController::OnSetTetheringEnabledFailure,
                     weak_ptr_factory_.GetWeakPtr(), enabled));
}

void HotspotController::OnSetTetheringEnabledSuccess(
    const bool& enabled,
    const std::string& result) {
  if (enabled) {
    CompleteEnableRequest(SetTetheringEnabledResultToMojom(result));
  } else {
    CompleteDisableRequest(SetTetheringEnabledResultToMojom(result));
  }
}

void HotspotController::OnSetTetheringEnabledFailure(
    const bool& enabled,
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Enable/disable tethering failed: " << error_name
                 << ", message: " << error_message;
  if (enabled) {
    CompleteEnableRequest(
        hotspot_config::mojom::HotspotControlResult::kShillOperationFailed);
  } else {
    CompleteDisableRequest(
        hotspot_config::mojom::HotspotControlResult::kShillOperationFailed);
  }
}

void HotspotController::CompleteEnableRequest(
    hotspot_config::mojom::HotspotControlResult result) {
  DCHECK(current_enable_request_);
  using hotspot_config::mojom::HotspotControlResult;

  if (result != HotspotControlResult::kAlreadyFulfilled) {
    HotspotMetricsHelper::RecordEnableHotspotLatency(
        current_enable_request_->enable_latency_timer->Elapsed());
  }

  hotspot_feature_usage_metrics_->RecordHotspotEnableAttempt(
      result == HotspotControlResult::kSuccess);

  HotspotMetricsHelper::RecordSetTetheringEnabledResult(
      /*enabled=*/true,
      current_enable_request_->abort ? HotspotControlResult::kAborted : result);

  NET_LOG(EVENT) << "Complete enable tethering request, result: " << result;

  if (wifi_turned_off_ && result != HotspotControlResult::kSuccess &&
      !current_enable_request_->abort) {
    // Turn Wifi back on if failed to enable hotspot.
    technology_state_controller_->SetTechnologiesEnabled(
        NetworkTypePattern::WiFi(), /*enabled=*/true,
        network_handler::ErrorCallback());
  }

  if (result == HotspotControlResult::kSuccess) {
    NotifyHotspotTurnedOn();
  }
  std::move(current_enable_request_->callback).Run(result);
  current_enable_request_.reset();
}

void HotspotController::CompleteDisableRequest(
    hotspot_config::mojom::HotspotControlResult result) {
  DCHECK(current_disable_request_);
  using hotspot_config::mojom::HotspotControlResult;

  HotspotMetricsHelper::RecordSetTetheringEnabledResult(
      /*enabled=*/false, result);

  NET_LOG(EVENT) << "Complete disable tethering request, result: " << result
                 << ", disable reason: "
                 << current_disable_request_->disable_reason.value();

  if (result == HotspotControlResult::kSuccess) {
    NotifyHotspotTurnedOff(current_disable_request_->disable_reason.value());
  }
  std::move(current_disable_request_->callback).Run(result);
  current_disable_request_.reset();
}

void HotspotController::SetPolicyAllowHotspot(bool allow_hotspot) {
  if (allow_hotspot_ == allow_hotspot) {
    return;
  }

  allow_hotspot_ = allow_hotspot;
  hotspot_capabilities_provider_->SetPolicyAllowed(allow_hotspot);
  if (!allow_hotspot && hotspot_state_handler_->GetHotspotState() !=
                            hotspot_config::mojom::HotspotState::kDisabled) {
    DisableHotspot(base::DoNothing(),
                   hotspot_config::mojom::DisableReason::kProhibitedByPolicy);
  }
}

void HotspotController::PrepareEnableWifi(
    base::OnceCallback<void(bool prepare_success)> callback) {
  if (hotspot_state_handler_->GetHotspotState() ==
          hotspot_config::mojom::HotspotState::kEnabled ||
      hotspot_state_handler_->GetHotspotState() ==
          hotspot_config::mojom::HotspotState::kEnabling ||
      current_enable_request_) {
    if (current_enable_request_) {
      current_enable_request_->abort = true;
    }
    DisableHotspot(
        base::BindOnce(&HotspotController::OnPrepareEnableWifiCompleted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        hotspot_config::mojom::DisableReason::kWifiEnabled);
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

void HotspotController::OnHotspotStatusChanged() {
  if (!wifi_turned_off_) {
    return;
  }

  hotspot_config::mojom::HotspotState hotspot_state =
      hotspot_state_handler_->GetHotspotState();
  if (hotspot_state != hotspot_config::mojom::HotspotState::kDisabled) {
    return;
  }

  absl::optional<hotspot_config::mojom::DisableReason> disable_reason =
      hotspot_state_handler_->GetDisableReason();
  if (disable_reason &&
      *disable_reason == hotspot_config::mojom::DisableReason::kRestart) {
    // No need to turn WiFi back on since the hotspot will restart immediately.
    return;
  }

  if (disable_reason) {
    NET_LOG(EVENT)
        << "Turning Wifi back on because hotspot is turned off due to "
        << *disable_reason;
  }
  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/true,
      network_handler::ErrorCallback());
  wifi_turned_off_ = false;
}

void HotspotController::OnDisableHotspotCompleteForRestart(
    hotspot_config::mojom::HotspotControlResult disable_result) {
  if (disable_result ==
      hotspot_config::mojom::HotspotControlResult::kAlreadyFulfilled) {
    // No need to start hotspot since it was not active.
    return;
  }
  if (disable_result != hotspot_config::mojom::HotspotControlResult::kSuccess) {
    NET_LOG(ERROR) << "Disable hotspot failed with result " << disable_result
                   << ", the new hotspot configuration is not applied.";
    return;
  }
  EnableHotspot(base::DoNothing());
}

void HotspotController::NotifyHotspotTurnedOn() {
  for (auto& observer : observer_list_) {
    observer.OnHotspotTurnedOn();
  }
}

void HotspotController::NotifyHotspotTurnedOff(
    hotspot_config::mojom::DisableReason disable_reason) {
  for (auto& observer : observer_list_) {
    observer.OnHotspotTurnedOff(disable_reason);
  }
}

}  //  namespace ash