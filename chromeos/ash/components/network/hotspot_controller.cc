// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_controller.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/hotspot_util.h"
#include "chromeos/ash/components/network/metrics/hotspot_feature_usage_metrics.h"
#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

using hotspot_config::mojom::DisableReason;
using hotspot_config::mojom::HotspotControlResult;
using hotspot_config::mojom::HotspotState;

HotspotController::HotspotControlRequest::HotspotControlRequest(
    bool enabled,
    std::optional<DisableReason> disable_reason,
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
  if (current_disable_request_) {
    NET_LOG(ERROR) << "Failed to enable hotspot as an existing disable "
                      "request is in progress";
    HotspotMetricsHelper::RecordSetTetheringEnabledResult(
        /*enabled=*/true, HotspotControlResult::kInvalid);
    return;
  }
  if (!current_enable_request_) {
    current_enable_request_ = std::make_unique<HotspotControlRequest>(
        /*enabled=*/true, /*disable_reason=*/std::nullopt, std::move(callback));
    if (hotspot_state_handler_->GetHotspotState() == HotspotState::kEnabled) {
      CompleteEnableRequest(HotspotControlResult::kAlreadyFulfilled);
      return;
    }
    current_enable_request_->enable_latency_timer = base::ElapsedTimer();
    hotspot_capabilities_provider_->CheckTetheringReadiness(
        base::BindOnce(&HotspotController::OnCheckTetheringReadiness,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void HotspotController::DisableHotspot(HotspotControlCallback callback,
                                       DisableReason disable_reason) {
  if (current_enable_request_) {
    current_enable_request_->abort = true;
    if (hotspot_state_handler_->GetHotspotState() == HotspotState::kEnabling) {
      current_disable_request_ = std::make_unique<HotspotControlRequest>(
          /*enabled=*/false, disable_reason, std::move(callback));
      PerformSetTetheringEnabled(/*enabled=*/false);
      return;
    }

    // If it goes here, it means disable hotspot request comes in before
    // calling into enable hotspot in Shill, e.g.: when still doing tethering
    // readiness, we'll just need to run the callback since hotspot is not
    // enabled yet.
    std::move(callback).Run(HotspotControlResult::kAlreadyFulfilled);
    return;
  }
  if (!current_disable_request_) {
    current_disable_request_ = std::make_unique<HotspotControlRequest>(
        /*enabled=*/false, disable_reason, std::move(callback));
    if (hotspot_state_handler_->GetHotspotState() == HotspotState::kDisabled) {
      CompleteDisableRequest(HotspotControlResult::kAlreadyFulfilled);
      return;
    }
    PerformSetTetheringEnabled(/*enabled=*/false);
  }
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

void HotspotController::OnCheckTetheringReadiness(
    HotspotCapabilitiesProvider::CheckTetheringReadinessResult result) {
  if (current_enable_request_->abort) {
    NET_LOG(ERROR) << "Aborting in check tethering readiness";
    CompleteEnableRequest(HotspotControlResult::kAborted);
    return;
  }
  if (result == HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
                    kUpstreamNetworkNotAvailable) {
    CompleteEnableRequest(HotspotControlResult::kUpstreamNotAvailable);
    return;
  }
  if (result !=
      HotspotCapabilitiesProvider::CheckTetheringReadinessResult::kReady) {
    CompleteEnableRequest(HotspotControlResult::kReadinessCheckFailed);
    return;
  }
  technology_state_controller_->PrepareEnableHotspot(
      base::BindOnce(&HotspotController::OnPrepareEnableHotspotCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HotspotController::OnPrepareEnableHotspotCompleted(bool prepare_success,
                                                        bool wifi_turned_off) {
  if (current_enable_request_->abort) {
    CompleteEnableRequest(HotspotControlResult::kAborted);
    return;
  }
  NET_LOG(EVENT) << "Prepare enable hotspot completed, success: "
                 << prepare_success << ", wifi turned off " << wifi_turned_off;
  wifi_turned_off_ = wifi_turned_off;
  if (!prepare_success) {
    CompleteEnableRequest(HotspotControlResult::kDisableWifiFailed);
    return;
  }
  PerformSetTetheringEnabled(/*enabled=*/true);
}

void HotspotController::PerformSetTetheringEnabled(bool enabled) {
  if (enabled && current_enable_request_->abort) {
    CompleteEnableRequest(HotspotControlResult::kAborted);
    return;
  }

  auto set_tethering_enabled_success_callback =
      base::BindOnce(&HotspotController::OnSetTetheringEnabledSuccess,
                     weak_ptr_factory_.GetWeakPtr(), enabled);
  auto set_tethering_enabled_failure_callback =
      base::BindOnce(&HotspotController::OnSetTetheringEnabledFailure,
                     weak_ptr_factory_.GetWeakPtr(), enabled);
  if (!features::IsWifiConcurrencyEnabled()) {
    ShillManagerClient::Get()->SetTetheringEnabled(
        enabled, std::move(set_tethering_enabled_success_callback),
        std::move(set_tethering_enabled_failure_callback));
    return;
  }

  if (enabled) {
    ShillManagerClient::Get()->EnableTethering(
        shill::WiFiInterfacePriority::USER_ASSERTED,
        std::move(set_tethering_enabled_success_callback),
        std::move(set_tethering_enabled_failure_callback));
  } else {
    ShillManagerClient::Get()->DisableTethering(
        std::move(set_tethering_enabled_success_callback),
        std::move(set_tethering_enabled_failure_callback));
  }
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
    CompleteEnableRequest(HotspotControlResult::kShillOperationFailed);
  } else {
    CompleteDisableRequest(HotspotControlResult::kShillOperationFailed);
  }
}

void HotspotController::CompleteEnableRequest(HotspotControlResult result) {
  DCHECK(current_enable_request_);
  if (result != HotspotControlResult::kAlreadyFulfilled) {
    HotspotMetricsHelper::RecordEnableHotspotLatency(
        current_enable_request_->enable_latency_timer->Elapsed());
  }

  hotspot_feature_usage_metrics_->RecordHotspotEnableAttempt(
      result == HotspotControlResult::kSuccess);

  const bool abort = current_enable_request_->abort;
  HotspotMetricsHelper::RecordSetTetheringEnabledResult(
      /*enabled=*/true, abort ? HotspotControlResult::kAborted : result);

  NET_LOG(EVENT) << "Complete enable tethering request, result: " << result
                 << ", wifi turned off: " << wifi_turned_off_
                 << ", abort: " << abort;

  if (result == HotspotControlResult::kSuccess) {
    NotifyHotspotTurnedOn();
  }
  std::move(current_enable_request_->callback).Run(result);
  current_enable_request_.reset();

  if (wifi_turned_off_ && result != HotspotControlResult::kSuccess && !abort) {
    // Turn Wifi back on if failed to enable hotspot.
    NET_LOG(EVENT) << "Turning WiFi back on due to failed to enable hotspot.";
    technology_state_controller_->SetTechnologiesEnabled(
        NetworkTypePattern::WiFi(), /*enabled=*/true,
        network_handler::ErrorCallback());
    wifi_turned_off_ = false;
  }
}

void HotspotController::CompleteDisableRequest(HotspotControlResult result) {
  DCHECK(current_disable_request_);

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
  if (!allow_hotspot &&
      hotspot_state_handler_->GetHotspotState() != HotspotState::kDisabled) {
    DisableHotspot(base::DoNothing(), DisableReason::kProhibitedByPolicy);
  }
}

void HotspotController::PrepareEnableWifi(
    base::OnceCallback<void(bool prepare_success)> callback) {
  if (hotspot_state_handler_->GetHotspotState() == HotspotState::kEnabled ||
      hotspot_state_handler_->GetHotspotState() == HotspotState::kEnabling ||
      current_enable_request_) {
    if (current_enable_request_) {
      current_enable_request_->abort = true;
    }
    DisableHotspot(
        base::BindOnce(&HotspotController::OnPrepareEnableWifiCompleted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        DisableReason::kWifiEnabled);
    return;
  }
  std::move(callback).Run(/*prepare_success=*/true);
}

void HotspotController::OnPrepareEnableWifiCompleted(
    base::OnceCallback<void(bool prepare_success)> callback,
    HotspotControlResult control_result) {
  if (control_result == HotspotControlResult::kSuccess) {
    std::move(callback).Run(/*prepare_success=*/true);
    return;
  }
  std::move(callback).Run(/*prepare_success=*/false);
}

void HotspotController::OnHotspotStatusChanged() {
  if (!wifi_turned_off_) {
    return;
  }

  HotspotState hotspot_state = hotspot_state_handler_->GetHotspotState();
  if (hotspot_state != HotspotState::kDisabled) {
    return;
  }

  std::optional<DisableReason> disable_reason =
      hotspot_state_handler_->GetDisableReason();
  if (disable_reason) {
    NET_LOG(EVENT)
        << "Turning Wifi back on because hotspot was turned off due to "
        << *disable_reason;
  } else {
    NET_LOG(EVENT) << "Turning Wifi back on because hotspot was turned off.";
  }

  technology_state_controller_->SetTechnologiesEnabled(
      NetworkTypePattern::WiFi(), /*enabled=*/true,
      network_handler::ErrorCallback());
  wifi_turned_off_ = false;
}

void HotspotController::NotifyHotspotTurnedOn() {
  for (auto& observer : observer_list_) {
    observer.OnHotspotTurnedOn();
  }
}

void HotspotController::NotifyHotspotTurnedOff(DisableReason disable_reason) {
  for (auto& observer : observer_list_) {
    observer.OnHotspotTurnedOff(disable_reason);
  }
}

}  //  namespace ash