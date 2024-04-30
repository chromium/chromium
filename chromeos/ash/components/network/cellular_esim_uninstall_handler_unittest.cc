// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_uninstall_handler.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/fake_network_connection_handler.h"
#include "chromeos/ash/components/network/fake_stub_cellular_networks_provider.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/policy_util.h"
#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"
#include "components/prefs/testing_pref_service.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kDefaultCellularDevicePath[] = "test_cellular_device";
const char kDefaultEuiccPath[] = "/org/chromium/Hermes/Euicc/0";
const char kDefaultEid[] = "12345678901234567890123456789012";

const char kTestCarrierProfilePath0[] = "/org/chromium/Hermes/Profile/123";
const char kTestNetworkServicePath0[] = "/service/cellular123";
const char kTestCellularIccid0[] = "100000000000000001";
const char kTestCellularActivationCode0[] = "smdp_address0";
const char kTestCellularNetworkName[] = "cellular0";
const char kTestProfileName[] = "TestCellularNetwork";
const char kTestProfileNickname[] = "TestCellularNetworkNick";
const char kTestServiceProvider[] = "Test Wireless";

const char kTestCarrierProfilePath1[] = "/org/chromium/Hermes/Profile/124";
const char kTestNetworkServicePath1[] = "/service/cellular124";
const char kTestCellularIccid1[] = "100000000000000002";
const char kTestCellularActivationCode1[] = "smdp_address1";

}  // namespace

class CellularESimUninstallHandlerTest : public testing::Test {
 public:
  CellularESimUninstallHandlerTest(const CellularESimUninstallHandlerTest&) =
      delete;
  CellularESimUninstallHandlerTest& operator=(
      const CellularESimUninstallHandlerTest&) = delete;

 protected:
  CellularESimUninstallHandlerTest() = default;
  ~CellularESimUninstallHandlerTest() override = default;

  // testing::Test
  void SetUp() override {
    shill_clients::InitializeFakes();
    hermes_clients::InitializeFakes();

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        network_state_handler_.get());
    network_configuration_handler_ =
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get(), network_device_handler_.get());
    network_connection_handler_ =
        std::make_unique<FakeNetworkConnectionHandler>();
    cellular_inhibitor_ = std::make_unique<CellularInhibitor>();
    cellular_inhibitor_->Init(network_state_handler_.get(),
                              network_device_handler_.get());
    cellular_esim_profile_handler_ =
        std::make_unique<TestCellularESimProfileHandler>();
    cellular_esim_profile_handler_->Init(network_state_handler_.get(),
                                         cellular_inhibitor_.get());
    managed_cellular_pref_handler_ =
        std::make_unique<ManagedCellularPrefHandler>();
    managed_cellular_pref_handler_->Init(network_state_handler_.get());
    ManagedCellularPrefHandler::RegisterLocalStatePrefs(
        device_prefs_.registry());
    managed_cellular_pref_handler_->SetDevicePrefs(&device_prefs_);

    cellular_esim_uninstall_handler_ =
        std::make_unique<CellularESimUninstallHandler>();
    cellular_esim_uninstall_handler_->Init(
        cellular_inhibitor_.get(), cellular_esim_profile_handler_.get(),
        managed_cellular_pref_handler_.get(),
        network_configuration_handler_.get(), network_connection_handler_.get(),
        network_state_handler_.get());

    stub_cellular_networks_provider_ =
        std::make_unique<FakeStubCellularNetworksProvider>();
    network_state_handler_->set_stub_cellular_networks_provider(
        stub_cellular_networks_provider_.get());
  }

  void TearDown() override {
    stub_cellular_networks_provider_.reset();
    cellular_esim_uninstall_handler_.reset();
    cellular_esim_profile_handler_.reset();
    cellular_inhibitor_.reset();
    managed_cellular_pref_handler_.reset();
    network_device_handler_.reset();
    network_state_handler_.reset();
    network_configuration_handler_.reset();
    network_connection_handler_.reset();
    hermes_clients::Shutdown();
    shill_clients::Shutdown();
  }

  void Init(bool is_first_profile_active = true) {
    auto first_profile_state = is_first_profile_active
                                   ? hermes::profile::State::kActive
                                   : hermes::profile::State::kInactive;

    ShillDeviceClient::Get()->GetTestInterface()->AddDevice(
        kDefaultCellularDevicePath, shill::kTypeCellular, "cellular1");
    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kDefaultEuiccPath), kDefaultEid, /*is_active=*/true,
        /*physical_slot=*/0);
    HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
        dbus::ObjectPath(kTestCarrierProfilePath0),
        dbus::ObjectPath(kDefaultEuiccPath), kTestCellularIccid0,
        kTestProfileName, kTestProfileNickname, kTestServiceProvider,
        kTestCellularActivationCode0, kTestNetworkServicePath0,
        first_profile_state, hermes::profile::ProfileClass::kOperational,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);

    // Setup as a managed profile by adding eSIM metadata to device prefs.
    managed_cellular_pref_handler_->AddESimMetadata(
        kTestCellularIccid0, kTestCellularNetworkName,
        policy_util::SmdxActivationCode(
            policy_util::SmdxActivationCode::Type::SMDP,
            kTestCellularActivationCode0));

    HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
        dbus::ObjectPath(kTestCarrierProfilePath1),
        dbus::ObjectPath(kDefaultEuiccPath), kTestCellularIccid1,
        kTestProfileName, kTestProfileNickname, kTestServiceProvider,
        kTestCellularActivationCode1, kTestNetworkServicePath1,
        hermes::profile::State::kInactive,
        hermes::profile::ProfileClass::kOperational,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);
    base::RunLoop().RunUntilIdle();

    ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
        kTestNetworkServicePath0, shill::kStateProperty,
        base::Value(shill::kStateOnline));
    base::RunLoop().RunUntilIdle();
  }

  void UninstallESim(base::RunLoop& run_loop,
                     const std::string& iccid,
                     const std::string& carrier_profile_path,
                     bool& status) {
    cellular_esim_uninstall_handler_->UninstallESim(
        iccid, dbus::ObjectPath(carrier_profile_path),
        dbus::ObjectPath(kDefaultEuiccPath),
        base::BindLambdaForTesting([&](bool status_result) {
          status = status_result;
          std::move(run_loop.QuitClosure()).Run();
        }));
  }

  void ResetEuiccMemory(base::RunLoop& run_loop, bool& status) {
    cellular_esim_uninstall_handler_->ResetEuiccMemory(
        dbus::ObjectPath(kDefaultEuiccPath),
        base::BindLambdaForTesting([&](bool status_result) {
          status = status_result;
          std::move(run_loop.QuitClosure()).Run();
        }));
  }

  void HandleNetworkDisconnect(bool should_fail) {
    // Run until the uninstallation state hits the disconnect on the
    // FakeNetworkConnectionHandler.
    base::RunLoop().RunUntilIdle();
    EXPECT_LT(0u, network_connection_handler_->disconnect_calls().size());
    if (should_fail) {
      network_connection_handler_->disconnect_calls()
          .front()
          .InvokeErrorCallback("disconnect_error");
    } else {
      network_connection_handler_->disconnect_calls()
          .front()
          .InvokeSuccessCallback();
    }
  }

  bool ESimServiceConfigExists(const std::string& service_path) {
    std::vector<std::string> profile_paths;
    ShillProfileClient::Get()
        ->GetTestInterface()
        ->GetProfilePathsContainingService(service_path, &profile_paths);
    return !profile_paths.empty();
  }

  bool HasESimMetadata(const std::string& iccid) {
    return managed_cellular_pref_handler_->GetESimMetadata(iccid) != nullptr;
  }

  void AddStub(const std::string& stub_iccid, const std::string& eid) {
    stub_cellular_networks_provider_->AddStub(stub_iccid, eid);
    network_state_handler_->SyncStubCellularNetworks();
  }

  void SetHasRefreshedProfiles(bool has_refreshed) {
    cellular_esim_profile_handler_->SetHasRefreshedProfilesForEuicc(
        kDefaultEid, dbus::ObjectPath(kDefaultEuiccPath), has_refreshed);
  }

  void ExpectResult(CellularESimUninstallHandler::UninstallESimResult result,
                    int expected_count = 1) {
    histogram_tester_.ExpectBucketCount(
        "Network.Cellular.ESim.UninstallProfile.OperationResult", result,
        expected_count);
  }

  void FastForward(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  uint8_t GetLastServiceCountRemovalForTesting() {
    return cellular_esim_uninstall_handler_
        ->last_service_count_removal_for_testing_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::HistogramTester histogram_tester_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
  std::unique_ptr<ManagedCellularPrefHandler> managed_cellular_pref_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<FakeNetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<CellularESimUninstallHandler>
      cellular_esim_uninstall_handler_;
  std::unique_ptr<FakeStubCellularNetworksProvider>
      stub_cellular_networks_provider_;
  TestingPrefServiceSimple device_prefs_;
};

TEST_F(CellularESimUninstallHandlerTest, Success) {
  Init();
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath0));

  base::RunLoop run_loop;
  bool status;
  UninstallESim(run_loop, kTestCellularIccid0, kTestCarrierProfilePath0,
                status);
  HandleNetworkDisconnect(/*should_fail=*/false);
  run_loop.Run();

  // Verify that the esim profile and shill service configuration are removed
  // properly.
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(
          dbus::ObjectPath(kDefaultEuiccPath));
  ASSERT_TRUE(euicc_properties);
  EXPECT_EQ(1u, euicc_properties->profiles().value().size());
  EXPECT_FALSE(ESimServiceConfigExists(kTestNetworkServicePath0));
  EXPECT_TRUE(status);
  EXPECT_FALSE(HasESimMetadata(kTestCellularIccid0));

  ExpectResult(CellularESimUninstallHandler::UninstallESimResult::kSuccess);
}

TEST_F(CellularESimUninstallHandlerTest, Success_AlreadyDisabled) {
  Init(/*is_first_profile_active=*/false);
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath0));

  base::RunLoop run_loop;
  bool status;
  UninstallESim(run_loop, kTestCellularIccid0, kTestCarrierProfilePath0,
                status);
  HandleNetworkDisconnect(/*should_fail=*/false);
  run_loop.Run();

  // Verify that the esim profile and shill service configuration are removed
  // properly.
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(
          dbus::ObjectPath(kDefaultEuiccPath));
  ASSERT_TRUE(euicc_properties);
  EXPECT_EQ(1u, euicc_properties->profiles().value().size());
  EXPECT_FALSE(ESimServiceConfigExists(kTestNetworkServicePath0));
  EXPECT_TRUE(status);
  EXPECT_FALSE(HasESimMetadata(kTestCellularIccid0));

  ExpectResult(CellularESimUninstallHandler::UninstallESimResult::kSuccess);
}

TEST_F(CellularESimUninstallHandlerTest, DisconnectFailure) {
  Init();
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath0));

  bool status;
  base::RunLoop run_loop;
  UninstallESim(run_loop, kTestCellularIccid0, kTestCarrierProfilePath0,
                status);
  HandleNetworkDisconnect(/*should_fail=*/true);
  run_loop.Run();
  EXPECT_FALSE(status);
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath0));
  EXPECT_TRUE(HasESimMetadata(kTestCellularIccid0));

  ExpectResult(
      CellularESimUninstallHandler::UninstallESimResult::kDisconnectFailed);
}

TEST_F(CellularESimUninstallHandlerTest, HermesFailure) {
  Init();
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath0));

  HermesEuiccClient::Get()->GetTestInterface()->QueueHermesErrorStatus(
      HermesResponseStatus::kErrorUnknown);
  bool status;
  base::RunLoop run_loop;
  UninstallESim(run_loop, kTestCellularIccid0, kTestCarrierProfilePath0,
                status);
  HandleNetworkDisconnect(/*should_fail=*/false);
  run_loop.Run();
  EXPECT_FALSE(status);
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath0));
  EXPECT_TRUE(HasESimMetadata(kTestCellularIccid0));

  ExpectResult(CellularESimUninstallHandler::UninstallESimResult::
                   kRefreshProfilesFailed);
}

TEST_F(CellularESimUninstallHandlerTest, MultipleRequests) {
  Init();
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath0));
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath1));

  // Make two uninstall requests back to back.
  bool status1, status2;
  base::RunLoop run_loop1, run_loop2;

  UninstallESim(run_loop1, kTestCellularIccid0, kTestCarrierProfilePath0,
                status1);
  UninstallESim(run_loop2, kTestCellularIccid1, kTestCarrierProfilePath1,
                status2);

  // Only the first profile is connected, so only one disconnect handler is
  // needed.
  HandleNetworkDisconnect(/*should_fail=*/false);

  run_loop1.Run();
  EXPECT_EQ(GetLastServiceCountRemovalForTesting(), 1);

  run_loop2.Run();
  EXPECT_EQ(GetLastServiceCountRemovalForTesting(), 1);

  // Verify that both requests succeeded.
  EXPECT_TRUE(status1);
  EXPECT_TRUE(status2);
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(
          dbus::ObjectPath(kDefaultEuiccPath));
  ASSERT_TRUE(euicc_properties);
  EXPECT_TRUE(euicc_properties->profiles().value().empty());
  EXPECT_FALSE(ESimServiceConfigExists(kTestNetworkServicePath0));
  EXPECT_FALSE(ESimServiceConfigExists(kTestNetworkServicePath1));
  EXPECT_FALSE(HasESimMetadata(kTestCellularIccid0));
  EXPECT_FALSE(HasESimMetadata(kTestCellularIccid1));

  ExpectResult(CellularESimUninstallHandler::UninstallESimResult::kSuccess,
               /*expected_count=*/2);
}

TEST_F(CellularESimUninstallHandlerTest, ResetEuiccMemory) {
  Init();
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath0));
  EXPECT_TRUE(ESimServiceConfigExists(kTestNetworkServicePath1));

  bool status;
  base::RunLoop run_loop;
  ResetEuiccMemory(run_loop, status);

  HandleNetworkDisconnect(/*should_fail=*/false);
  FastForward(CellularESimUninstallHandler::kNetworkListWaitTimeout);

  run_loop.Run();

  // Verify that both profiles were removed successfully.
  EXPECT_TRUE(status);
  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(
          dbus::ObjectPath(kDefaultEuiccPath));
  ASSERT_TRUE(euicc_properties);
  EXPECT_TRUE(euicc_properties->profiles().value().empty());
  EXPECT_FALSE(ESimServiceConfigExists(kTestNetworkServicePath0));
  EXPECT_FALSE(ESimServiceConfigExists(kTestNetworkServicePath1));
  EXPECT_FALSE(HasESimMetadata(kTestCellularIccid0));
  EXPECT_FALSE(HasESimMetadata(kTestCellularIccid1));
  EXPECT_EQ(GetLastServiceCountRemovalForTesting(), 2);

  ExpectResult(CellularESimUninstallHandler::UninstallESimResult::kSuccess,
               /*expected_count=*/1);
}

TEST_F(CellularESimUninstallHandlerTest, StubCellularNetwork) {
  Init();

  // Remove shill eSIM service and add a corresponding stub service.
  ShillServiceClient::Get()->GetTestInterface()->RemoveService(
      kTestNetworkServicePath0);
  base::RunLoop().RunUntilIdle();
  AddStub(kTestCellularIccid0, kDefaultEid);

  // Verify that removing the eSIM profile succeeds.
  base::RunLoop run_loop;
  bool success;
  UninstallESim(run_loop, kTestCellularIccid0, kTestCarrierProfilePath0,
                success);
  run_loop.Run();
  EXPECT_TRUE(success);

  ExpectResult(CellularESimUninstallHandler::UninstallESimResult::kSuccess);
}

}  // namespace ash
