// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_policy_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/hermes/hermes_clients.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/network/cellular_connection_handler.h"
#include "chromeos/network/cellular_esim_installer.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/fake_network_connection_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/test_cellular_esim_profile_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kTestEuiccPath[] = "/org/chromium/Hermes/Euicc/0";
const char kTestEuiccPath2[] = "/org/chromium/Hermes/Euicc/1";
const char kTestEid[] = "12345678901234567890123456789012";
const char kTestEid2[] = "12345678901234567890123456789000";

}  // namespace

class CellularPolicyHandlerTest : public testing::Test {
 protected:
  CellularPolicyHandlerTest() = default;
  ~CellularPolicyHandlerTest() override = default;

  // testing::Test
  void SetUp() override {
    shill_clients::InitializeFakes();
    hermes_clients::InitializeFakes();

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        network_state_handler_.get());
    network_connection_handler_ =
        std::make_unique<FakeNetworkConnectionHandler>();
    cellular_inhibitor_ = std::make_unique<CellularInhibitor>();
    cellular_inhibitor_->Init(network_state_handler_.get(),
                              network_device_handler_.get());
    cellular_esim_profile_handler_ =
        std::make_unique<TestCellularESimProfileHandler>();
    cellular_esim_profile_handler_->Init(network_state_handler_.get(),
                                         cellular_inhibitor_.get());
    cellular_connection_handler_ =
        std::make_unique<CellularConnectionHandler>();
    cellular_connection_handler_->Init(network_state_handler_.get(),
                                       cellular_inhibitor_.get(),
                                       cellular_esim_profile_handler_.get());
    cellular_esim_installer_ = std::make_unique<CellularESimInstaller>();
    cellular_esim_installer_->Init(
        cellular_connection_handler_.get(), cellular_inhibitor_.get(),
        network_connection_handler_.get(), network_state_handler_.get());
    cellular_policy_handler_ = std::make_unique<CellularPolicyHandler>();
    cellular_policy_handler_->Init(cellular_esim_installer_.get());
    SetupEuicc();
  }

  void SetupEuicc() {
    HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEid, /*is_active=*/true,
        /*physical_slot=*/0);
    base::RunLoop().RunUntilIdle();
  }

  // testing::Test
  void TearDown() override {
    cellular_policy_handler_.reset();
    cellular_esim_installer_.reset();
    cellular_esim_profile_handler_.reset();
    cellular_connection_handler_.reset();
    cellular_inhibitor_.reset();
    network_device_handler_.reset();
    network_state_handler_.reset();
    network_connection_handler_.reset();

    hermes_clients::Shutdown();
    shill_clients::Shutdown();
  }

  void InstallESimPolicy(const std::string& activation_code,
                         bool expect_install_success) {
    cellular_policy_handler_->InstallESim(activation_code);
    FastForwardProfileRefreshDelay();
    base::RunLoop().RunUntilIdle();

    if (expect_install_success)
      EXPECT_LE(1u, network_connection_handler_->connect_calls().size());
    else
      EXPECT_LE(0u, network_connection_handler_->connect_calls().size());

    base::RunLoop().RunUntilIdle();
  }

  void FastForwardProfileRefreshDelay() {
    const base::TimeDelta kProfileRefreshCallbackDelay =
        base::TimeDelta::FromMilliseconds(150);
    // Connect can result in two profile refresh calls before and after
    // enabling profile. Fast forward by delay after refresh.
    task_environment_.FastForwardBy(2 * kProfileRefreshCallbackDelay);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<CellularConnectionHandler> cellular_connection_handler_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
  std::unique_ptr<FakeNetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<CellularESimInstaller> cellular_esim_installer_;
  std::unique_ptr<CellularPolicyHandler> cellular_policy_handler_;
};

TEST_F(CellularPolicyHandlerTest, InstallProfileSuccess) {
  // Verify esim profile get installed successfully when installing policy esim
  // with a fake SMDP address.
  InstallESimPolicy(HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/true);
}

TEST_F(CellularPolicyHandlerTest, InstallProfileFailure) {
  // Verify esim profile doesn't get installed when installing policy esim
  // with a invalid SMDP address.
  InstallESimPolicy(/*activation_code=*/std::string(),
                    /*expect_install_success=*/false);
}

TEST_F(CellularPolicyHandlerTest, InstallOnExternalEUICC) {
  // Verify esim profile get installed successfully when installing policy esim
  // on the external EUICC.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kCellularUseExternalEuicc);
  HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
      dbus::ObjectPath(kTestEuiccPath2), kTestEid2, /*is_active=*/true,
      /*physical_slot=*/1);
  base::RunLoop().RunUntilIdle();
  InstallESimPolicy(HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/true);
}

TEST_F(CellularPolicyHandlerTest, InstallNoEUICCAvailable) {
  // Verify esim profile doesn't get installed when installing policy esim
  // with no available EUICC.
  HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
  base::RunLoop().RunUntilIdle();
  InstallESimPolicy(HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/false);
}

}  // namespace chromeos
