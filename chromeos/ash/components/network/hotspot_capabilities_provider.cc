// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"

#include "base/containers/contains.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/hotspot_allowed_flag_handler.h"
#include "chromeos/ash/components/network/hotspot_util.h"
#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

// Convert the base::Value::List type of |allowed_security_modes_in_shill| to
// the corresponding mojom enum and update the value to the
// |allowed_security_modes|.
void UpdateAllowedSecurityList(
    std::vector<hotspot_config::mojom::WiFiSecurityMode>&
        allowed_security_modes,
    const base::Value::List& allowed_security_modes_in_shill) {
  allowed_security_modes.clear();
  for (const base::Value& allowed_security : allowed_security_modes_in_shill) {
    allowed_security_modes.push_back(
        ShillSecurityToMojom(allowed_security.GetString()));
  }
}

bool IsDisallowedByPlatformCapabilities(
    hotspot_config::mojom::HotspotAllowStatus allow_status) {
  using HotspotAllowStatus = hotspot_config::mojom::HotspotAllowStatus;
  return allow_status == HotspotAllowStatus::kDisallowedNoCellularUpstream ||
         allow_status == HotspotAllowStatus::kDisallowedNoWiFiDownstream ||
         allow_status == HotspotAllowStatus::kDisallowedNoWiFiSecurityModes;
}

HotspotCapabilitiesProvider::CheckTetheringReadinessResult
ShillResultToReadinessResult(const std::string& result) {
  if (result == shill::kTetheringReadinessReady) {
    return HotspotCapabilitiesProvider::CheckTetheringReadinessResult::kReady;
  }
  if (result == shill::kTetheringReadinessUpstreamNetworkNotAvailable) {
    return HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
        kUpstreamNetworkNotAvailable;
  }
  if (result == shill::kTetheringReadinessNotAllowedByCarrier) {
    return HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
        kNotAllowedByCarrier;
  }
  if (result == shill::kTetheringReadinessNotAllowedOnFw) {
    return HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
        kNotAllowedOnFW;
  }
  if (result == shill::kTetheringReadinessNotAllowedOnVariant) {
    return HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
        kNotAllowedOnVariant;
  }
  if (result == shill::kTetheringReadinessNotAllowedUserNotEntitled) {
    return HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
        kNotAllowedUserNotEntitled;
  }
  if (result == shill::kTetheringReadinessNotAllowed) {
    return HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
        kNotAllowed;
  }
  NET_LOG(ERROR) << "Unexpected check tethering readiness result: " << result;
  return HotspotCapabilitiesProvider::CheckTetheringReadinessResult::
      kUnknownResult;
}

}  // namespace

HotspotCapabilitiesProvider::HotspotCapabilities::HotspotCapabilities(
    const hotspot_config::mojom::HotspotAllowStatus allow_status)
    : allow_status(allow_status) {}

HotspotCapabilitiesProvider::HotspotCapabilities::~HotspotCapabilities() =
    default;

HotspotCapabilitiesProvider::HotspotCapabilitiesProvider() = default;

HotspotCapabilitiesProvider::~HotspotCapabilitiesProvider() {
  ResetNetworkStateHandler();

  if (ShillManagerClient::Get()) {
    ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
  }
}

void HotspotCapabilitiesProvider::Init(
    NetworkStateHandler* network_state_handler,
    HotspotAllowedFlagHandler* hotspot_allowed_flag_handler) {
  network_state_handler_ = network_state_handler;
  network_state_handler_observer_.Observe(network_state_handler_.get());

  hotspot_allowed_flag_handler_ = hotspot_allowed_flag_handler;

  // Add as an observer here so that new hotspot state updated after this call
  // are recognized.
  ShillManagerClient::Get()->AddPropertyChangedObserver(this);
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&HotspotCapabilitiesProvider::OnManagerProperties,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HotspotCapabilitiesProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HotspotCapabilitiesProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool HotspotCapabilitiesProvider::HasObserver(Observer* observer) const {
  return observer_list_.HasObserver(observer);
}

const HotspotCapabilitiesProvider::HotspotCapabilities&
HotspotCapabilitiesProvider::GetHotspotCapabilities() const {
  return hotspot_capabilities_;
}

void HotspotCapabilitiesProvider::OnPropertyChanged(const std::string& key,
                                                    const base::Value& value) {
  if (key == shill::kTetheringCapabilitiesProperty) {
    UpdateHotspotCapabilities(value.GetDict());
  }
}

// The hotspot capabilities is re-calculated when a cellular network connection
// state is changed.
void HotspotCapabilitiesProvider::NetworkConnectionStateChanged(
    const NetworkState* network) {
  using HotspotAllowStatus = hotspot_config::mojom::HotspotAllowStatus;
  // Only check the Cellular connectivity as the upstream technology
  if (!network->Matches(NetworkTypePattern::Cellular())) {
    return;
  }

  // Exit early if the platform capabilities doesn't support hotspot.
  if (IsDisallowedByPlatformCapabilities(hotspot_capabilities_.allow_status)) {
    return;
  }

  if (!policy_allow_hotspot_) {
    SetHotspotAllowStatus(HotspotAllowStatus::kDisallowedByPolicy);
    return;
  }

  if (network->IsOnline()) {
    CheckTetheringReadiness(base::DoNothing());
    return;
  }

  SetHotspotAllowStatus(HotspotAllowStatus::kDisallowedNoMobileData);
}

void HotspotCapabilitiesProvider::OnShuttingDown() {
  ResetNetworkStateHandler();
}

void HotspotCapabilitiesProvider::ResetNetworkStateHandler() {
  if (!network_state_handler_) {
    return;
  }
  network_state_handler_observer_.Reset();
  network_state_handler_ = nullptr;
}

void HotspotCapabilitiesProvider::OnManagerProperties(
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    NET_LOG(ERROR)
        << "HotspotCapabilitiesProvider: Failed to get manager properties.";
    return;
  }

  const base::Value::Dict* capabilities =
      properties->FindDict(shill::kTetheringCapabilitiesProperty);
  if (!capabilities) {
    NET_LOG(EVENT) << "HotspotCapabilitiesProvider: No dict value for: "
                   << shill::kTetheringCapabilitiesProperty;
  } else {
    UpdateHotspotCapabilities(*capabilities);
  }
}

void HotspotCapabilitiesProvider::UpdateHotspotCapabilities(
    const base::Value::Dict& capabilities) {
  using HotspotAllowStatus = hotspot_config::mojom::HotspotAllowStatus;

  const base::Value::List* upstream_technologies =
      capabilities.FindList(shill::kTetheringCapUpstreamProperty);
  if (!upstream_technologies) {
    NET_LOG(ERROR) << "No list value for: "
                   << shill::kTetheringCapUpstreamProperty << " in "
                   << shill::kTetheringCapabilitiesProperty;
    SetHotspotAllowStatus(HotspotAllowStatus::kDisallowedNoCellularUpstream);
    return;
  }

  if (!base::Contains(*upstream_technologies, shill::kTypeCellular)) {
    SetHotspotAllowStatus(HotspotAllowStatus::kDisallowedNoCellularUpstream);
    return;
  }

  const base::Value::List* downstream_technologies =
      capabilities.FindList(shill::kTetheringCapDownstreamProperty);
  if (!downstream_technologies) {
    NET_LOG(ERROR) << "No list value for: "
                   << shill::kTetheringCapDownstreamProperty << " in "
                   << shill::kTetheringCapabilitiesProperty;
    SetHotspotAllowStatus(HotspotAllowStatus::kDisallowedNoWiFiDownstream);
    return;
  }

  if (!base::Contains(*downstream_technologies, shill::kTypeWifi)) {
    SetHotspotAllowStatus(HotspotAllowStatus::kDisallowedNoWiFiDownstream);
    return;
  }

  // Update allowed security modes for WiFi downstream
  const base::Value::List* allowed_security_modes_in_shill =
      capabilities.FindList(shill::kTetheringCapSecurityProperty);
  if (!allowed_security_modes_in_shill) {
    NET_LOG(ERROR) << "No list value for: "
                   << shill::kTetheringCapSecurityProperty << " in "
                   << shill::kTetheringCapabilitiesProperty;
    SetHotspotAllowStatus(HotspotAllowStatus::kDisallowedNoWiFiSecurityModes);
    return;
  }

  UpdateAllowedSecurityList(hotspot_capabilities_.allowed_security_modes,
                            *allowed_security_modes_in_shill);
  if (hotspot_capabilities_.allowed_security_modes.empty()) {
    SetHotspotAllowStatus(HotspotAllowStatus::kDisallowedNoWiFiSecurityModes);
    return;
  }

  if (!policy_allow_hotspot_) {
    SetHotspotAllowStatus(HotspotAllowStatus::kDisallowedByPolicy);
    return;
  }

  // Check if there's a connected cellular network
  const NetworkState* connected_cellular_network =
      network_state_handler_->ConnectedNetworkByType(
          NetworkTypePattern::Cellular());
  if (!connected_cellular_network) {
    SetHotspotAllowStatus(HotspotAllowStatus::kDisallowedNoMobileData);
    return;
  }

  CheckTetheringReadiness(base::DoNothing());
}

void HotspotCapabilitiesProvider::CheckTetheringReadiness(
    CheckTetheringReadinessCallback callback) {
  hotspot_allowed_flag_handler_->UpdateFlags();
  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ShillManagerClient::Get()->CheckTetheringReadiness(
      base::BindOnce(&HotspotCapabilitiesProvider::OnCheckReadinessSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_split.first)),
      base::BindOnce(&HotspotCapabilitiesProvider::OnCheckReadinessFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_split.second)));
}

void HotspotCapabilitiesProvider::OnCheckReadinessSuccess(
    CheckTetheringReadinessCallback callback,
    const std::string& result) {
  using HotspotAllowStatus = hotspot_config::mojom::HotspotAllowStatus;
  NET_LOG(EVENT) << "Check tethering readiness result: " << result;
  CheckTetheringReadinessResult readiness_result =
      ShillResultToReadinessResult(result);
  if (result == shill::kTetheringReadinessReady) {
    SetHotspotAllowStatus(HotspotAllowStatus::kAllowed);
  } else if (result == shill::kTetheringReadinessUpstreamNetworkNotAvailable) {
    SetHotspotAllowStatus(HotspotAllowStatus::kDisallowedNoMobileData);
  } else {
    SetHotspotAllowStatus(HotspotAllowStatus::kDisallowedReadinessCheckFail);
  }

  HotspotMetricsHelper::RecordCheckTetheringReadinessResult(readiness_result);
  std::move(callback).Run(readiness_result);
}

void HotspotCapabilitiesProvider::OnCheckReadinessFailure(
    CheckTetheringReadinessCallback callback,
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Check tethering readiness failed, error name: "
                 << error_name << ", message: " << error_message;
  SetHotspotAllowStatus(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedReadinessCheckFail);
  HotspotMetricsHelper::RecordCheckTetheringReadinessResult(
      CheckTetheringReadinessResult::kShillOperationFailed);
  std::move(callback).Run(CheckTetheringReadinessResult::kShillOperationFailed);
}

void HotspotCapabilitiesProvider::SetHotspotAllowStatus(
    hotspot_config::mojom::HotspotAllowStatus new_allow_status) {
  if (hotspot_capabilities_.allow_status == new_allow_status &&
      new_allow_status == hotspot_config::mojom::HotspotAllowStatus::kAllowed) {
    return;
  }

  hotspot_capabilities_.allow_status = new_allow_status;
  NotifyHotspotCapabilitiesChanged();
}

void HotspotCapabilitiesProvider::NotifyHotspotCapabilitiesChanged() {
  for (auto& observer : observer_list_) {
    observer.OnHotspotCapabilitiesChanged();
  }
}

void HotspotCapabilitiesProvider::SetPolicyAllowed(bool allowed) {
  policy_allow_hotspot_ = allowed;
  if (!policy_allow_hotspot_ &&
      !IsDisallowedByPlatformCapabilities(hotspot_capabilities_.allow_status)) {
    SetHotspotAllowStatus(
        hotspot_config::mojom::HotspotAllowStatus::kDisallowedByPolicy);
    return;
  }
  if (policy_allow_hotspot_ &&
      hotspot_capabilities_.allow_status ==
          hotspot_config::mojom::HotspotAllowStatus::kDisallowedByPolicy) {
    CheckTetheringReadiness(base::DoNothing());
  }
}

}  // namespace ash
