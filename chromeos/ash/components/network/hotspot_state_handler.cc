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

size_t GetActiveClientCount(const base::Value::Dict& status) {
  const base::Value::List* active_clients =
      status.FindList(shill::kTetheringStatusClientsProperty);
  if (!active_clients) {
    NET_LOG(ERROR) << shill::kTetheringStatusClientsProperty << " not found in "
                   << shill::kTetheringStatusProperty;
    return 0;
  }
  return active_clients->size();
}

}  // namespace

HotspotStateHandler::HotspotStateHandler() = default;

HotspotStateHandler::~HotspotStateHandler() {
  if (ShillManagerClient::Get()) {
    ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
  }
  if (LoginState::IsInitialized()) {
    LoginState::Get()->RemoveObserver(this);
  }
}

void HotspotStateHandler::Init() {
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
    absl::optional<base::Value::Dict> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "Error getting Shill manager properties.";
    std::move(callback).Run(
        hotspot_config::mojom::SetHotspotConfigResult::kSuccess);
    return;
  }
  const base::Value::Dict* shill_tethering_config =
      properties->FindDict(shill::kTetheringConfigProperty);
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
  if (key == shill::kTetheringStatusProperty) {
    UpdateHotspotStatus(value.GetDict());
  } else if (key == shill::kProfilesProperty) {
    // Shill initializes the tethering config with random value and signals
    // "Profiles" property changes when the tethering config is fully loaded
    // from persistent storage.
    ShillManagerClient::Get()->GetProperties(
        base::BindOnce(&HotspotStateHandler::UpdateHotspotConfigAndRunCallback,
                       weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));
  }
}

void HotspotStateHandler::OnManagerProperties(
    absl::optional<base::Value::Dict> properties) {
  if (!properties) {
    NET_LOG(ERROR) << "HotspotStateHandler: Failed to get manager properties.";
    return;
  }

  const base::Value::Dict* status =
      properties->FindDict(shill::kTetheringStatusProperty);
  if (!status) {
    NET_LOG(EVENT) << "HotspotStateHandler: No dict value for: "
                   << shill::kTetheringStatusProperty;
  } else {
    UpdateHotspotStatus(*status);
  }
}

void HotspotStateHandler::UpdateHotspotStatus(const base::Value::Dict& status) {
  const std::string* state =
      status.FindString(shill::kTetheringStatusStateProperty);
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

void HotspotStateHandler::NotifyHotspotStatusChanged() {
  for (auto& observer : observer_list_)
    observer.OnHotspotStatusChanged();
}

}  // namespace ash
