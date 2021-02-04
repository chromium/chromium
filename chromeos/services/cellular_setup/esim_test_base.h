// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_TEST_BASE_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_TEST_BASE_H_

#include "base/test/task_environment.h"
#include "chromeos/services/cellular_setup/public/cpp/esim_manager_test_observer.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {
class SingleThreadTaskEnvironment;
}  // namespace test
}  // namespace base

namespace chromeos {

class CellularInhibitor;
class NetworkStateHandler;
class NetworkDeviceHandler;
class NetworkConfigurationHandler;
class FakeNetworkConnectionHandler;
class CellularESimUninstallHandler;
class CellularESimProfileHandler;

namespace cellular_setup {

class ESimManager;

// Base class for testing eSIM mojo impl classes.
class ESimTestBase : public testing::Test {
 public:
  static const char* kTestEuiccPath;
  static const char* kTestEid;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Creates a test euicc.
  void SetupEuicc();

  // Returns list of available euiccs under the test ESimManager.
  std::vector<mojo::PendingRemote<mojom::Euicc>> GetAvailableEuiccs();

  // Returns euicc with given |eid| under the test ESimManager.
  mojo::Remote<mojom::Euicc> GetEuiccForEid(const std::string& eid);

 protected:
  ESimTestBase();
  ~ESimTestBase() override;

  ESimManager* esim_manager() { return esim_manager_.get(); }
  ESimManagerTestObserver* observer() { return observer_.get(); }

 private:
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<FakeNetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<CellularESimUninstallHandler>
      cellular_esim_uninstall_handler_;
  std::unique_ptr<CellularESimProfileHandler> cellular_esim_profile_handler_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ESimManager> esim_manager_;
  std::unique_ptr<ESimManagerTestObserver> observer_;
};

}  // namespace cellular_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_ESIM_TEST_BASE_H_