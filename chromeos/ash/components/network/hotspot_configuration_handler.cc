// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_configuration_handler.h"

#include "base/containers/contains.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/hotspot_util.h"
#include "chromeos/ash/components/network/metrics/hotspot_metrics_helper.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

HotspotConfigurationHandler::HotspotConfigurationHandler() = default;

HotspotConfigurationHandler::~HotspotConfigurationHandler() {
  if (ShillManagerClient::Get()) {
    ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
  }
  if (LoginState::IsInitialized()) {
    LoginState::Get()->RemoveObserver(this);
  }
}

void HotspotConfigurationHandler::Init() {
  if (LoginState::IsInitialized()) {
    LoginState::Get()->AddObserver(this);
  }

  // Add as an observer here because shill will signal "Profiles" property
  // change when tethering config is fully loaded from persistent storage.
  ShillManagerClient::Get()->AddPropertyChangedObserver(this);
  if (LoginState::IsInitialized()) {
    LoggedInStateChanged();
  }
}

void HotspotConfigurationHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HotspotConfigurationHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool HotspotConfigurationHandler::HasObserver(Observer* observer) const {
  return observer_list_.HasObserver(observer);
}

hotspot_config::mojom::HotspotConfigPtr
HotspotConfigurationHandler::GetHotspotConfig() const {
  if (!hotspot_config_) {
    return nullptr;
  }

  return ShillTetheringConfigToMojomConfig(*hotspot_config_);
}

void HotspotConfigurationHandler::SetHotspotConfig(
    hotspot_config::mojom::HotspotConfigPtr mojom_config,
    SetHotspotConfigCallback callback) {
  using SetHotspotConfigResult = hotspot_config::mojom::SetHotspotConfigResult;

  if (!LoginState::Get()->IsUserLoggedIn()) {
    NET_LOG(ERROR) << "Could not set hotspot config without login first.";
    HotspotMetricsHelper::RecordSetHotspotConfigResult(
        SetHotspotConfigResult::kFailedNotLogin);
    std::move(callback).Run(SetHotspotConfigResult::kFailedNotLogin);
    return;
  }

  if (!mojom_config) {
    NET_LOG(ERROR) << "Invalid hotspot configurations.";
    HotspotMetricsHelper::RecordSetHotspotConfigResult(
        SetHotspotConfigResult::kFailedInvalidConfiguration);
    std::move(callback).Run(
        SetHotspotConfigResult::kFailedInvalidConfiguration);
    return;
  }

  base::Value::Dict shill_tethering_config =
      MojomConfigToShillConfig(std::move(mojom_config));
  auto callback_split = base::SplitOnceCallback(std::move(callback));
  ShillManagerClient::Get()->SetProperty(
      shill::kTetheringConfigProperty,
      base::Value(std::move(shill_tethering_config)),
      base::BindOnce(&HotspotConfigurationHandler::OnSetHotspotConfigSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_split.first)),
      base::BindOnce(&HotspotConfigurationHandler::OnSetHotspotConfigFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_split.second)));
}

void HotspotConfigurationHandler::OnSetHotspotConfigSuccess(
    SetHotspotConfigCallback callback) {
  HotspotMetricsHelper::RecordSetHotspotConfigResult(
      hotspot_config::mojom::SetHotspotConfigResult::kSuccess);
  ShillManagerClient::Get()->GetProperties(base::BindOnce(
      &HotspotConfigurationHandler::UpdateHotspotConfigAndRunCallback,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HotspotConfigurationHandler::OnSetHotspotConfigFailure(
    SetHotspotConfigCallback callback,
    const std::string& error_name,
    const std::string& error_message) {
  using SetHotspotConfigResult = hotspot_config::mojom::SetHotspotConfigResult;

  NET_LOG(ERROR) << "Error setting hotspot config, error name:" << error_name
                 << ", message" << error_message;

  HotspotMetricsHelper::RecordSetHotspotConfigResult(
      SetHotspotConfigResult::kFailedShillOperation, error_name);
  std::move(callback).Run(
      error_name == shill::kErrorResultInvalidArguments
          ? SetHotspotConfigResult::kFailedInvalidConfiguration
          : SetHotspotConfigResult::kFailedShillOperation);
}

void HotspotConfigurationHandler::LoggedInStateChanged() {
  if (!LoginState::Get()->IsUserLoggedIn()) {
    if (hotspot_config_) {
      hotspot_config_ = std::nullopt;
      NotifyHotspotConfigurationChanged();
    }
    return;
  }
  ShillManagerClient::Get()->GetProperties(base::BindOnce(
      &HotspotConfigurationHandler::UpdateHotspotConfigAndRunCallback,
      weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));
}

void HotspotConfigurationHandler::UpdateHotspotConfigAndRunCallback(
    SetHotspotConfigCallback callback,
    std::optional<base::Value::Dict> properties) {
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
  NotifyHotspotConfigurationChanged();
}

void HotspotConfigurationHandler::OnPropertyChanged(const std::string& key,
                                                    const base::Value& value) {
  if (key == shill::kProfilesProperty) {
    // Shill initializes the tethering config with random value and signals
    // "Profiles" property changes when the tethering config is fully loaded
    // from persistent storage.
    ShillManagerClient::Get()->GetProperties(base::BindOnce(
        &HotspotConfigurationHandler::UpdateHotspotConfigAndRunCallback,
        weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));
  }
}

void HotspotConfigurationHandler::NotifyHotspotConfigurationChanged() {
  for (auto& observer : observer_list_) {
    observer.OnHotspotConfigurationChanged();
  }
}

}  // namespace ash
