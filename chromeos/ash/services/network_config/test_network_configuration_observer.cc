// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/network_config/test_network_configuration_observer.h"

#include "chromeos/components/onc/onc_utils.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::network_config {

namespace {

constexpr char kUIDataKeyUserSettings[] = "user_settings";

}

TestNetworkConfigurationObserver::TestNetworkConfigurationObserver(
    NetworkConfigurationHandler* network_configuration_handler) {
  DCHECK(network_configuration_handler);
  network_configuration_observation_.Observe(network_configuration_handler);
}

TestNetworkConfigurationObserver::~TestNetworkConfigurationObserver() = default;

void TestNetworkConfigurationObserver::OnConfigurationModified(
    const std::string& service_path,
    const std::string& network_guid,
    const base::Value::Dict* set_properties) {
  if (!set_properties)
    return;

  ++on_configuration_modified_call_count_;

  const base::Value* ui_data = set_properties->Find(shill::kUIDataProperty);
  if (!ui_data) {
    return;
  }
  const std::string* ui_data_str = ui_data->GetIfString();
  if (!ui_data_str) {
    return;
  }
  std::optional<base::Value::Dict> ui_data_dict =
      chromeos::onc::ReadDictionaryFromJson(*ui_data_str);
  if (!ui_data_dict.has_value()) {
    return;
  }
  const base::Value::Dict* user_settings =
      ui_data_dict->FindDict(kUIDataKeyUserSettings);
  if (!user_settings) {
    return;
  }
  user_settings_.insert_or_assign(network_guid, user_settings->Clone());
}

const base::Value::Dict* TestNetworkConfigurationObserver::GetUserSettings(
    const std::string& network_guid) const {
  auto it = user_settings_.find(network_guid);
  if (it == user_settings_.end())
    return nullptr;

  return &it->second;
}

unsigned int
TestNetworkConfigurationObserver::GetOnConfigurationModifiedCallCount() const {
  return on_configuration_modified_call_count_;
}

}  // namespace ash::network_config
