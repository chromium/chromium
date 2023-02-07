// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NETWORK_CONFIG_TEST_NETWORK_CONFIGURATION_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_NETWORK_CONFIG_TEST_NETWORK_CONFIGURATION_OBSERVER_H_

#include <unordered_map>

#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_observer.h"

namespace ash::network_config {

class TestNetworkConfigurationObserver : public NetworkConfigurationObserver {
 public:
  explicit TestNetworkConfigurationObserver(
      NetworkConfigurationHandler* network_configuration_handler);
  ~TestNetworkConfigurationObserver() override;

  TestNetworkConfigurationObserver(const TestNetworkConfigurationObserver&) =
      delete;
  TestNetworkConfigurationObserver& operator=(
      const TestNetworkConfigurationObserver&) = delete;

  // NetworkConfigurationObserver
  void OnConfigurationModified(
      const std::string& service_path,
      const std::string& guid,
      const base::Value::Dict* set_properties) override;

  const base::Value::Dict* GetUserSettings(const std::string& guid) const;
  unsigned int GetOnConfigurationModifiedCallCount() const;

 private:
  std::unordered_map<std::string, base::Value::Dict> user_settings_;
  unsigned int on_configuration_modified_call_count_ = 0;

  base::ScopedObservation<NetworkConfigurationHandler,
                          NetworkConfigurationObserver>
      network_configuration_observation_{this};
};

}  // namespace ash::network_config

#endif  // CHROMEOS_ASH_SERVICES_NETWORK_CONFIG_TEST_NETWORK_CONFIGURATION_OBSERVER_H_
