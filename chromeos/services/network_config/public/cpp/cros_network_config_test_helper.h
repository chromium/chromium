// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_HELPER_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_HELPER_H_

#include <memory>

#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace network_config {

class CrosNetworkConfig;

// Helper for tests which need a CrosNetworkConfig service interface.
class CrosNetworkConfigTestHelper {
 public:
  // Default constructor for unit tests.
  CrosNetworkConfigTestHelper();

  // Constructor for when a ManagedNetworkConfigurationHandler must be
  // separately initialized via Initialize(ManagedNetworkConfigurationHandler*).
  explicit CrosNetworkConfigTestHelper(bool initialize);

  CrosNetworkConfigTestHelper(const CrosNetworkConfigTestHelper&) = delete;
  CrosNetworkConfigTestHelper& operator=(const CrosNetworkConfigTestHelper&) =
      delete;

  ~CrosNetworkConfigTestHelper();

  mojom::NetworkStatePropertiesPtr CreateStandaloneNetworkProperties(
      const std::string& id,
      mojom::NetworkType type,
      mojom::ConnectionStateType connection_state,
      int signal_strength);

  NetworkStateTestHelper& network_state_helper() {
    return network_state_helper_;
  }

  NetworkDeviceHandler* network_device_handler() {
    return network_state_helper_.network_device_handler();
  }

  CellularInhibitor* cellular_inhibitor() { return cellular_inhibitor_.get(); }

  void Initialize(
      ManagedNetworkConfigurationHandler* network_configuration_handler);

 protected:
  // Called in |~CrosNetworkConfigTestHelper()| to set the global network config
  // to nullptr and destroy cros_network_config_impl_.
  void Shutdown();

  NetworkStateTestHelper network_state_helper_{
      /*use_default_devices_and_services=*/false};
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<CrosNetworkConfig> cros_network_config_impl_;
};

}  // namespace network_config
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace network_config {
using ::chromeos::network_config::CrosNetworkConfigTestHelper;
}  // namespace network_config
}  // namespace ash

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_HELPER_H_
