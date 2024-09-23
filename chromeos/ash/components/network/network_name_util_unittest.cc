// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_name_util.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kTestEuiccPath[] = "euicc_path";
const char kTestIccid[] = "iccid";
const char kTestProfileName[] = "test_profile_name";
const char kTestProfileNickname[] = "test_profile_nickname";
const char kTestServiceProviderName[] = "test_service_provider_name";
const char kTestEidName[] = "eid";
const char kTestEthName[] = "test_eth_name";
const char kTestNameFromShill[] = "shill_network_name";

const char kTestCellularDevicePath[] = "device/cellular1";
const char kTestESimCellularServicePath[] = "/service/cellular1";
const char kTestEthServicePath[] = "/service/eth0";

}  // namespace

class NetworkNameUtilTest : public testing::Test {
 public:
  NetworkNameUtilTest() {}
  ~NetworkNameUtilTest() override = default;

  // testing::Test:
  void SetUp() override {
    cellular_inhibitor_ = std::make_unique<CellularInhibitor>();
    cellular_esim_profile_handler_ =
        std::make_unique<TestCellularESimProfileHandler>();

    network_state_test_helper_.hermes_manager_test()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEidName, /*is_active=*/true,
        /*physical_slot=*/0);
    network_state_test_helper_.device_test()->AddDevice(
        kTestCellularDevicePath, shill::kTypeCellular, "fake_cellular_device");
    cellular_esim_profile_handler_->Init(
        network_state_test_helper_.network_state_handler(),
        cellular_inhibitor_.get());
    base::RunLoop().RunUntilIdle();
  }

  void AddESimProfile(const std::string& name,
                      const std::string& nickname,
                      const std::string& service_provider_name,
                      hermes::profile::State state,
                      const std::string& service_path) {
    network_state_test_helper_.hermes_euicc_test()->AddCarrierProfile(
        dbus::ObjectPath(service_path), dbus::ObjectPath(kTestEuiccPath),
        kTestIccid, name, nickname, service_provider_name, "activation_code",
        service_path, state, hermes::profile::ProfileClass::kOperational,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);
    base::RunLoop().RunUntilIdle();
  }

  void AddEthernet() {
    ShillServiceClient::TestInterface* service_test =
        network_state_test_helper_.service_test();
    service_test->AddService(kTestEthServicePath, "test_guid1", kTestEthName,
                             shill::kTypeEthernet, shill::kStateIdle, true);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
};

TEST_F(NetworkNameUtilTest, EsimNetworkGetNetworkName) {
  AddESimProfile(kTestProfileName, kTestProfileNickname,
                 kTestServiceProviderName, hermes::profile::State::kActive,
                 kTestESimCellularServicePath);

  const NetworkState* network =
      network_state_test_helper_.network_state_handler()->GetNetworkState(
          kTestESimCellularServicePath);

  std::string name = network_name_util::GetNetworkName(
      cellular_esim_profile_handler_.get(), network);

  EXPECT_EQ(name, kTestProfileNickname);
}

TEST_F(NetworkNameUtilTest, EsimNetworNetworkNamePriority) {
  AddESimProfile("", "", kTestServiceProviderName,
                 hermes::profile::State::kActive, kTestESimCellularServicePath);
  const NetworkState* network =
      network_state_test_helper_.network_state_handler()->GetNetworkState(
          kTestESimCellularServicePath);

  std::string name = network_name_util::GetNetworkName(
      cellular_esim_profile_handler_.get(), network);

  EXPECT_EQ(name, kTestServiceProviderName);
}

TEST_F(NetworkNameUtilTest, EthernetNetworkGetNetworkName) {
  AddEthernet();
  const NetworkState* network =
      network_state_test_helper_.network_state_handler()->GetNetworkState(
          kTestEthServicePath);

  std::string name = network_name_util::GetNetworkName(
      cellular_esim_profile_handler_.get(), network);

  EXPECT_EQ(name, kTestEthName);
}

TEST_F(NetworkNameUtilTest, NameComesFromHermes) {
  AddESimProfile(kTestProfileName, kTestProfileNickname,
                 kTestServiceProviderName, hermes::profile::State::kActive,
                 kTestESimCellularServicePath);

  // Change the network's name in Shill. Now, Hermes and Shill have different
  // names associated with the profile.
  network_state_test_helper_.SetServiceProperty(
      kTestESimCellularServicePath, shill::kNameProperty,
      base::Value(kTestNameFromShill));
  base::RunLoop().RunUntilIdle();

  const NetworkState* network =
      network_state_test_helper_.network_state_handler()->GetNetworkState(
          kTestESimCellularServicePath);

  std::string name = network_name_util::GetNetworkName(
      cellular_esim_profile_handler_.get(), network);

  EXPECT_EQ(name, kTestProfileNickname);
}

}  // namespace ash
