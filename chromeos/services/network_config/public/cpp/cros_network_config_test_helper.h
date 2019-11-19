// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_HELPER_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class NetworkDeviceHandler;

namespace network_config {

class CrosNetworkConfig;

// Helper for tests which need a CrosNetworkConfig service interface.
class CrosNetworkConfigTestHelper {
 public:
  // Default constructor for unit tests.
  CrosNetworkConfigTestHelper();
  ~CrosNetworkConfigTestHelper();

  NetworkStateTestHelper& network_state_helper() {
    return *network_state_helper_;
  }

 private:
  std::unique_ptr<NetworkStateTestHelper> network_state_helper_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<CrosNetworkConfig> cros_network_config_impl_;

  DISALLOW_COPY_AND_ASSIGN(CrosNetworkConfigTestHelper);
};

}  // namespace network_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_HELPER_H_
