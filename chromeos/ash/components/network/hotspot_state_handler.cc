// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_state_handler.h"

#include "base/containers/contains.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/hotspot_util.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

size_t GetActiveClientCount(const base::Value& status) {
  const base::Value* active_clients =
      status.FindListKey(shill::kTetheringStatusClientsProperty);
  if (!active_clients) {
    NET_LOG(ERROR) << shill::kTetheringStatusClientsProperty << " not found in "
                   << shill::kTetheringStatusProperty;
    return 0;
  }
  return active_clients->GetList().size();
}

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

}  // namespace

HotspotStateHandler::HotspotCapabilities::HotspotCapabilities(
    const hotspot_config::mojom::HotspotAllowStatus allow_status)
    : allow_status(allow_status) {}

HotspotStateHandler::HotspotCapabilities::~HotspotCapabilities() = default;

HotspotStateHandler::HotspotStateHandler() = default;

HotspotStateHandler::~HotspotStateHandler() {
  ResetNetworkStateHandler();

  if (ShillManagerClient::Get()) {
    ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
  }
  if (LoginState::IsInitialized()) {
    LoginState::Get()->RemoveObserver(this);
  }
}

void HotspotStateHandler::Init(NetworkStateHandler* network_state_handler) {
  network_state_handler_ = network_state_handler;
  network_state_handler_observer_.Observe(network_state_handler_);

  if (LoginState::IsInitialized()) {
    LoginState::Get()->AddObserver(this);
  }
  // Add as an observer here so that new hotspot state updated after this call
  // are recognized.
  ShillManagerClient::Get()->AddPropertyChangedObserver(this);
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&HotspotStateHandler::OnManagerProperties,
                     weak_ptr_factory_.GetWeakPtr()));
  if (LoginState::IsInitialized())
    LoggedInStateChanged();
}

void HotspotStateHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HotspotStateHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool HotspotStateHandler::HasObserver(Observer* observer) const {
  return observer_list_.HasObserver(observer);
}

const hotspot_config::mojom::HotspotState&
HotspotStateHandler::GetHotspotState() const {
  return hotspot_state_;
}

size_t HotspotStateHandler::GetHotspotActiveClientCount() const {
  return active_client_count_;
}

const HotspotStateHandler::HotspotCapabilities&
HotspotStateHandler::GetHotspotCapabilities() const {
  return hotspot_capabilities_;
}

hotspot_config::mojom::HotspotConfigPtr HotspotStateHandler::GetHotspotConfig()
    const {
  if (!hotspot_config_)
    return nullptr;

  return ShillTetheringConfigToMojomConfig(*hotspot_config_);
}

void HotspotStateHandler::SetHotspotConfig(
    hotspot_config::mojom::HotspotConfigPtr mojom_config,
    SetHotspotConfigCallback callback) {
  using SetHotspotConfigResult = hotspot_config::mojom::SetHotspotConfigResult;

  if (!LoginState::Get()->IsUserLoggedIn()) {
    NET_LOG(ERROR) << "Could not set hotspot config without login first.";
    std::move(callback).Run(SetHotspotConfigResult::kFailedNotLogin);
    return;
  }

  if (!mojom_config) {
    NET_LOG(ERROR) << "Invalid hotspot configurations.";
    std::move(callback).Run(
        SetHotspotConfigResult::kFailedInvalidConfiguration);
    return;
  }

  base::Value shill_tethering_config =
      MojomConfigToShillConfig(std::move(mojom_config));
  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ShillManagerClient::Get()->SetProperty(
      shill::kTetheringConfigProperty, std::move(shill_tethering_config),
      base::BindOnce(&HotspotStateHandler::OnSetHotspotConfigSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_split.first)),
      base::BindOnce(&HotspotStateHandler::OnSetHotspotConfigFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_split.second)));
}

void HotspotStateHandler::OnSetHotspotConfigSuccess(
    SetHotspotConfigCallback callback) {
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&HotspotStateHandler::UpdateHotspotConfigAndRunCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HotspotStateHandler::OnSetHotspotConfigFailure(
    SetHotspotConfigCallback callback,
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Error setting hotspot config, error name:" << error_name
                 << ", message" << error_message;
  std::move(callback).Run(hotspot_config::mojom::SetHotspotConfigResult::
                              kFailedInvalidConfiguration);
}

void HotspotStateHandler::LoggedInStateChanged() {
  if (!LoginState::Get()->IsUserLoggedIn()) {
    if (hotspot_config_) {
      hotspot_config_ = absl::nullopt;
      NotifyHotspotStatusChanged();
    }
    return;
  }
  ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&HotspotStateHandler::UpdateHotspotConfigAndRunCallback,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));
}

void HotspotStateHandler::UpdateHotspotConfigAndRunCallback(
    SetHotspotConfigCallback callback,
    absl::optional<base::Value> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "Error getting Shill manager properties.";
    std::move(callback).Run(
        hotspot_config::mojom::SetHotspotConfigResult::kSuccess);
    return;
  }
  const base::Value* shill_tethering_config =
      properties->FindDictKey(shill::kTetheringConfigProperty);
  if (!shill_tethering_config) {
    NET_LOG(ERROR) << "Error getting " << shill::kTetheringConfigProperty
                   << " in Shill manager properties";
    std::move(callback).Run(
        hotspot_config::mojom::SetHotspotConfigResult::kSuccess);
    return;
  }

  hotspot_config_ = shill_tethering_config->Clone();
  std::move(callback).Run(
      hotspot_config::mojom::SetHotspotConfigResult::kSuccess);
  NotifyHotspotStatusChanged();
}

void HotspotStateHandler::OnPropertyChanged(const std::string& key,
                                            const base::Value& value) {
  if (key == shill::kTetheringStatusProperty)
    UpdateHotspotStatus(value);
  else if (key == shill::kTetheringCapabilitiesProperty)
    UpdateHotspotCapabilities(value);
}

// The hotspot capabilities is re-calculated when a cellular network connection
// state is changed.
void HotspotStateHandler::NetworkConnectionStateChanged(
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

  if (!network->IsConnectingOrConnected()) {
    // The cellular network got disconnected.
    SetHotspotCapablities(HotspotAllowStatus::kDisallowedNoMobileData);
    return;
  }

  if (network->IsConnectedState()) {
    ShillManagerClient::Get()->CheckTetheringReadiness(
        base::BindOnce(&HotspotStateHandler::OnCheckReadinessSuccess,
                       weak_ptr_factory_.GetWeakPtr(), base::DoNothing()),
        base::BindOnce(&HotspotStateHandler::OnCheckReadinessFailure,
                       weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));
  }
}

void HotspotStateHandler::OnShuttingDown() {
  ResetNetworkStateHandler();
}

void HotspotStateHandler::ResetNetworkStateHandler() {
  if (!network_state_handler_) {
    return;
  }
  network_state_handler_observer_.Reset();
  network_state_handler_ = nullptr;
}

void HotspotStateHandler::OnManagerProperties(
    absl::optional<base::Value> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "HotspotStateHandler: Failed to get manager properties.";
    return;
  }

  const base::Value* status =
      properties->FindDictKey(shill::kTetheringStatusProperty);
  if (!status) {
    NET_LOG(EVENT) << "HotspotStateHandler: No dict value for: "
                   << shill::kTetheringStatusProperty;
  } else {
    UpdateHotspotStatus(*status);
  }

  const base::Value* capabilities =
      properties->FindDictKey(shill::kTetheringCapabilitiesProperty);
  if (!capabilities) {
    NET_LOG(EVENT) << "HotspotStateHandler: No dict value for: "
                   << shill::kTetheringCapabilitiesProperty;
  } else {
    UpdateHotspotCapabilities(*capabilities);
  }
}

void HotspotStateHandler::UpdateHotspotStatus(const base::Value& status) {
  const std::string* state =
      status.FindStringKey(shill::kTetheringStatusStateProperty);
  if (!state) {
    NET_LOG(EVENT) << "HotspotStateHandler: No string value for: "
                   << shill::kTetheringStatusStateProperty << " in "
                   << shill::kTetheringStatusProperty;
    return;
  }

  hotspot_config::mojom::HotspotState mojom_state =
      ShillTetheringStateToMojomState(*state);
  if (mojom_state != hotspot_state_) {
    hotspot_state_ = mojom_state;
    NotifyHotspotStatusChanged();
  }

  if (mojom_state != hotspot_config::mojom::HotspotState::kEnabled) {
    active_client_count_ = 0;
    return;
  }
  size_t active_client_count = GetActiveClientCount(status);
  if (active_client_count == active_client_count_)
    return;

  active_client_count_ = active_client_count;
  NotifyHotspotStatusChanged();
}

void HotspotStateHandler::UpdateHotspotCapabilities(
    const base::Value& capabilities) {
  using HotspotAllowStatus = hotspot_config::mojom::HotspotAllowStatus;

  const base::Value* upstream_technologies =
      capabilities.FindListKey(shill::kTetheringCapUpstreamProperty);
  if (!upstream_technologies) {
    NET_LOG(ERROR) << "No list value for: "
                   << shill::kTetheringCapUpstreamProperty << " in "
                   << shill::kTetheringCapabilitiesProperty;
    SetHotspotCapablities(HotspotAllowStatus::kDisallowedNoCellularUpstream);
    return;
  }

  if (!base::Contains(upstream_technologies->GetList(),
                      base::Value(shill::kTypeCellular))) {
    SetHotspotCapablities(HotspotAllowStatus::kDisallowedNoCellularUpstream);
    return;
  }

  const base::Value* downstream_technologies =
      capabilities.FindListKey(shill::kTetheringCapDownstreamProperty);
  if (!downstream_technologies) {
    NET_LOG(ERROR) << "No list value for: "
                   << shill::kTetheringCapDownstreamProperty << " in "
                   << shill::kTetheringCapabilitiesProperty;
    SetHotspotCapablities(HotspotAllowStatus::kDisallowedNoWiFiDownstream);
    return;
  }

  if (!base::Contains(downstream_technologies->GetList(),
                      base::Value(shill::kTypeWifi))) {
    SetHotspotCapablities(HotspotAllowStatus::kDisallowedNoWiFiDownstream);
    return;
  }

  // Update allowed security modes for WiFi downstream
  const base::Value* allowed_security_modes_in_shill =
      capabilities.FindListKey(shill::kTetheringCapSecurityProperty);
  if (!allowed_security_modes_in_shill) {
    NET_LOG(ERROR) << "No list value for: "
                   << shill::kTetheringCapSecurityProperty << " in "
                   << shill::kTetheringCapabilitiesProperty;
    SetHotspotCapablities(HotspotAllowStatus::kDisallowedNoWiFiSecurityModes);
    return;
  }

  UpdateAllowedSecurityList(hotspot_capabilities_.allowed_security_modes,
                            allowed_security_modes_in_shill->GetList());
  if (hotspot_capabilities_.allowed_security_modes.empty()) {
    SetHotspotCapablities(HotspotAllowStatus::kDisallowedNoWiFiSecurityModes);
    return;
  }

  // Check if there's a connected cellular network
  const NetworkState* connected_cellular_network =
      network_state_handler_->ConnectedNetworkByType(
          NetworkTypePattern::Cellular());
  if (!connected_cellular_network) {
    SetHotspotCapablities(HotspotAllowStatus::kDisallowedNoMobileData);
    return;
  }

  CheckTetheringReadiness(base::DoNothing());
}

void HotspotStateHandler::CheckTetheringReadiness(
    CheckTetheringReadinessCallback callback) {
  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ShillManagerClient::Get()->CheckTetheringReadiness(
      base::BindOnce(&HotspotStateHandler::OnCheckReadinessSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_split.first)),
      base::BindOnce(&HotspotStateHandler::OnCheckReadinessFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_split.second)));
}

void HotspotStateHandler::OnCheckReadinessSuccess(
    CheckTetheringReadinessCallback callback,
    const std::string& result) {
  using HotspotAllowStatus = hotspot_config::mojom::HotspotAllowStatus;

  if (result == shill::kTetheringReadinessReady) {
    SetHotspotCapablities(HotspotAllowStatus::kAllowed);
    std::move(callback).Run(CheckTetheringReadinessResult::kReady);
    return;
  }
  if (result == shill::kTetheringReadinessNotAllowed) {
    SetHotspotCapablities(HotspotAllowStatus::kDisallowedReadinessCheckFail);
    std::move(callback).Run(CheckTetheringReadinessResult::kNotAllowed);
    return;
  }
  NET_LOG(ERROR) << "Unexpected check tethering readiness result: " << result;
  std::move(callback).Run(CheckTetheringReadinessResult::kNotAllowed);
}

void HotspotStateHandler::OnCheckReadinessFailure(
    CheckTetheringReadinessCallback callback,
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Check tethering readiness failed, error name: "
                 << error_name << ", message: " << error_message;
  SetHotspotCapablities(
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedReadinessCheckFail);
  std::move(callback).Run(CheckTetheringReadinessResult::kShillOperationFailed);
}

void HotspotStateHandler::SetHotspotCapablities(
    hotspot_config::mojom::HotspotAllowStatus new_allow_status) {
  if (hotspot_capabilities_.allow_status == new_allow_status)
    return;

  hotspot_capabilities_.allow_status = new_allow_status;
  NotifyHotspotCapabilitiesChanged();
}

void HotspotStateHandler::SetPolicyAllowHotspot(bool allow) {
  // TODO (jiajunz)
}

void HotspotStateHandler::NotifyHotspotStatusChanged() {
  for (auto& observer : observer_list_)
    observer.OnHotspotStatusChanged();
}

void HotspotStateHandler::NotifyHotspotCapabilitiesChanged() {
  for (auto& observer : observer_list_)
    observer.OnHotspotCapabilitiesChanged();
}

}  // namespace ash