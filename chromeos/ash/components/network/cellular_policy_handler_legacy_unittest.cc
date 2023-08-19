// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_policy_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/cellular_connection_handler.h"
#include "chromeos/ash/components/network/cellular_esim_installer.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/fake_network_connection_handler.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"
#include "chromeos/components/onc/onc_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kTestEuiccPath[] = "/org/chromium/Hermes/Euicc/0";
const char kTestEuiccPath2[] = "/org/chromium/Hermes/Euicc/1";
const char kTestESimProfilePath[] =
    "/org/chromium/Hermes/Euicc/1/Profile/1000000000000000002";
const char kTestServicePath[] = "/service/cellular102";
const char kTestEid[] = "12345678901234567890123456789012";
const char kTestEid2[] = "12345678901234567890123456789000";
const char kCellularGuid[] = "cellular_guid";
const char kICCID[] = "1000000000000000002";
const char kInstallViaPolicyOperationHistogram[] =
    "Network.Cellular.ESim.Policy.ESimInstall.OperationResult";
const char kInstallViaPolicyInitialOperationHistogram[] =
    "Network.Cellular.ESim.Policy.ESimInstall.OperationResult.InitialAttempt";
const char kInstallViaPolicyRetryOperationHistogram[] =
    "Network.Cellular.ESim.Policy.ESimInstall.OperationResult.Retry";
const char kUnmanagedCellularServicePath[] = "cellular_service_path";
const char kUnmanagedCellularGuid[] = "unmanaged_cellular_guid";
const char kUnmanagedCellularName[] = "unmanaged_cellular";
const char kWifiServicePath[] = "wifi_service_path";
const char kWifiGuid[] = "wifi_guid";
const char kWifiName[] = "wifi";
const base::TimeDelta kInstallationRetryDelay = base::Days(1);
const base::TimeDelta kInstallationRetryDelayExponential = base::Minutes(12);

void CheckShillConfiguration(bool is_installed) {
  std::string service_path =
      ShillServiceClient::Get()->GetTestInterface()->FindServiceMatchingGUID(
          kCellularGuid);
  const base::Value::Dict* properties =
      ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
          service_path);

  if (!is_installed) {
    EXPECT_EQ(properties, nullptr);
    return;
  }
  const std::string* guid = properties->FindString(shill::kGuidProperty);
  EXPECT_EQ(kCellularGuid, *guid);
  // UIData should not be empty if cellular service is configured.
  const std::string* ui_data_value =
      properties->FindString(shill::kUIDataProperty);
  EXPECT_NE(*ui_data_value, std::string());
  const std::string* iccid = properties->FindString(shill::kIccidProperty);
  EXPECT_EQ(kICCID, *iccid);
}

std::string GenerateCellularPolicy(
    const std::string& smdp_address,
    absl::optional<std::string> iccid = absl::nullopt) {
  if (!iccid) {
    return base::StringPrintf(
        R"({"GUID": "%s", "Type": "Cellular",
                          "Name": "cellular1",
                          "Cellular": { "SMDPAddress": "%s"}})",
        kCellularGuid, smdp_address.c_str());
  }

  return base::StringPrintf(
      R"({"GUID": "%s", "Type": "Cellular",
                        "Name": "cellular1",
                        "Cellular": { "ICCID": "%s", "SMDPAddress": "%s"}})",
      kCellularGuid, iccid->c_str(), smdp_address.c_str());
}

}  // namespace

class CellularPolicyHandlerLegacyTest : public testing::Test {
 protected:
  CellularPolicyHandlerLegacyTest() = default;
  ~CellularPolicyHandlerLegacyTest() override = default;

  // testing::Test
  void SetUp() override {
    shill_clients::InitializeFakes();
    hermes_clients::InitializeFakes();

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        network_state_handler_.get());
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
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
        network_connection_handler_.get(), network_profile_handler_.get(),
        network_state_handler_.get());
    network_configuration_handler_ =
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get(), network_device_handler_.get());
    managed_network_configuration_handler_ =
        ManagedNetworkConfigurationHandler::InitializeForTesting(
            network_state_handler_.get(), network_profile_handler_.get(),
            network_device_handler_.get(), network_configuration_handler_.get(),
            /*UIProxyConfigService=*/nullptr);
    managed_cellular_pref_handler_ =
        std::make_unique<ManagedCellularPrefHandler>();
    managed_cellular_pref_handler_->Init(network_state_handler_.get());
    ManagedCellularPrefHandler::RegisterLocalStatePrefs(
        device_prefs_.registry());
    managed_cellular_pref_handler_->SetDevicePrefs(&device_prefs_);
    cellular_policy_handler_ = std::make_unique<CellularPolicyHandler>();
    cellular_policy_handler_->Init(
        cellular_esim_profile_handler_.get(), cellular_esim_installer_.get(),
        cellular_inhibitor_.get(), network_profile_handler_.get(),
        network_state_handler_.get(), managed_cellular_pref_handler_.get(),
        managed_network_configuration_handler_.get());
  }

  void SetupEuicc() {
    HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEid, /*is_active=*/true,
        /*physical_slot=*/0);
    cellular_esim_profile_handler_->SetHasRefreshedProfilesForEuicc(
        kTestEid, dbus::ObjectPath(kTestEuiccPath), /*has_refreshed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetupEuicc2() {
    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath2), kTestEid2, /*is_active=*/true,
        /*physical_slot=*/1);
    cellular_esim_profile_handler_->SetHasRefreshedProfilesForEuicc(
        kTestEid2, dbus::ObjectPath(kTestEuiccPath2), /*has_refreshed=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void SetupESimProfile() {
    HermesEuiccClient::Get()->GetTestInterface()->AddCarrierProfile(
        dbus::ObjectPath(kTestESimProfilePath),
        dbus::ObjectPath(kTestEuiccPath), kICCID, /*name=*/std::string(),
        /*nickname=*/std::string(),
        /*service_provider=*/std::string(), /*activation_code=*/std::string(),
        kTestServicePath, hermes::profile::State::kInactive,
        hermes::profile::ProfileClass::kOperational,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithoutService);
    base::RunLoop().RunUntilIdle();
  }

  // testing::Test
  void TearDown() override {
    cellular_policy_handler_.reset();
    cellular_esim_installer_.reset();
    cellular_esim_profile_handler_.reset();
    cellular_connection_handler_.reset();
    cellular_inhibitor_.reset();
    network_configuration_handler_.reset();
    managed_network_configuration_handler_.reset();
    managed_cellular_pref_handler_.reset();
    network_profile_handler_.reset();
    network_device_handler_.reset();
    network_state_handler_.reset();
    network_connection_handler_.reset();

    hermes_clients::Shutdown();
    shill_clients::Shutdown();
  }

  void InstallESimPolicy(const std::string& onc_json,
                         const std::string& activation_code,
                         bool expect_install_success,
                         bool auto_connect = false) {
    absl::optional<base::Value::Dict> policy =
        chromeos::onc::ReadDictionaryFromJson(onc_json);
    ASSERT_TRUE(policy.has_value());
    cellular_policy_handler_->InstallESim(activation_code, *policy);
    FastForwardProfileRefreshDelay();
    base::RunLoop().RunUntilIdle();

    if (!expect_install_success) {
      EXPECT_LE(0u, network_connection_handler_->connect_calls().size());
      return;
    }
    FastForwardAutoConnectWaiting(auto_connect);
    base::RunLoop().RunUntilIdle();
    if (!auto_connect) {
      EXPECT_LE(1u, network_connection_handler_->connect_calls().size());
      network_connection_handler_->connect_calls()
          .back()
          .InvokeSuccessCallback();
    }
    base::RunLoop().RunUntilIdle();
  }

  void CheckIccidSmdpPairInPref(bool is_installed) {
    const std::string* smdp_address =
        managed_cellular_pref_handler_->GetSmdpAddressFromIccid(kICCID);
    if (!is_installed) {
      EXPECT_FALSE(smdp_address);
      return;
    }
    ASSERT_TRUE(smdp_address);
    EXPECT_FALSE(smdp_address->empty());
  }

  void FastForwardProfileRefreshDelay() {
    const base::TimeDelta kProfileRefreshCallbackDelay =
        base::Milliseconds(150);
    // Connect can result in two profile refresh calls before and after
    // enabling profile. Fast forward by delay after refresh.
    FastForwardBy(2 * kProfileRefreshCallbackDelay);
  }

  void FastForwardAutoConnectWaiting(bool auto_connect) {
    if (auto_connect) {
      task_environment_.FastForwardBy(base::Seconds(10));
      ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
          kTestServicePath, shill::kStateProperty,
          base::Value(shill::kStateOnline));
      return;
    }

    task_environment_.FastForwardBy(
        CellularConnectionHandler::kWaitingForAutoConnectTimeout);
  }

  void FastForwardBy(base::TimeDelta delay) {
    task_environment_.FastForwardBy(delay);
  }

 protected:
  // This struct is used to simplify checking the metrics associated with the
  // tests below.
  struct ExpectedHistogramState {
    size_t success_initial_count = 0u;
    size_t success_retry_count = 0u;
    size_t inhibit_failed_initial_count = 0u;
    size_t inhibit_failed_retry_count = 0u;
    size_t hermes_install_failed_initial_count = 0u;
    size_t hermes_install_failed_retry_count = 0u;
  };

  void CheckHistogram(const char* histogram,
                      size_t success_count,
                      size_t inhibit_failed_count,
                      size_t hermes_install_failed_count) {
    using InstallESimProfileResult =
        CellularESimInstaller::InstallESimProfileResult;
    histogram_tester_.ExpectBucketCount(histogram,
                                        InstallESimProfileResult::kSuccess,
                                        /*expected_count=*/success_count);
    histogram_tester_.ExpectBucketCount(
        histogram, InstallESimProfileResult::kInhibitFailed,
        /*expected_count=*/inhibit_failed_count);
    histogram_tester_.ExpectBucketCount(
        histogram, InstallESimProfileResult::kHermesInstallFailed,
        /*expected_count=*/hermes_install_failed_count);
  }

  void CheckHistogramState(const ExpectedHistogramState& state) {
    CheckHistogram(
        kInstallViaPolicyOperationHistogram,
        /*success_count=*/state.success_initial_count +
            state.success_retry_count,
        /*inhibit_failed_count=*/state.inhibit_failed_initial_count +
            state.inhibit_failed_retry_count,
        /*hermes_install_failed=*/state.hermes_install_failed_initial_count +
            state.hermes_install_failed_retry_count);
    CheckHistogram(
        kInstallViaPolicyInitialOperationHistogram,
        /*success_count=*/state.success_initial_count,
        /*inhibit_failed_count=*/state.inhibit_failed_initial_count,
        /*hermes_install_failed=*/state.hermes_install_failed_initial_count);
    CheckHistogram(
        kInstallViaPolicyRetryOperationHistogram,
        /*success_count=*/state.success_retry_count,
        /*inhibit_failed_count=*/state.inhibit_failed_retry_count,
        /*hermes_install_failed=*/state.hermes_install_failed_retry_count);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::HistogramTester histogram_tester_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<CellularConnectionHandler> cellular_connection_handler_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
  std::unique_ptr<FakeNetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<CellularESimInstaller> cellular_esim_installer_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<ManagedCellularPrefHandler> managed_cellular_pref_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  std::unique_ptr<CellularPolicyHandler> cellular_policy_handler_;
  TestingPrefServiceSimple device_prefs_;
};

TEST_F(CellularPolicyHandlerLegacyTest, InstallProfileSuccess) {
  SetupEuicc();
  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode());
  // Verify esim profile get installed successfully when installing policy esim
  // with a fake SMDP address.
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/true,
                    /*auto_connect=*/true);
  CheckShillConfiguration(/*is_installed=*/true);
  CheckIccidSmdpPairInPref(/*is_installed=*/true);

  CheckHistogramState({.success_initial_count = 1});
}

TEST_F(CellularPolicyHandlerLegacyTest, InstallWaitForDeviceState) {
  SetupEuicc();
  ShillManagerClient::Get()->GetTestInterface()->ClearDevices();
  base::RunLoop().RunUntilIdle();

  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode());
  // Verify the configuration is created automatically after device state
  // becomes available.
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/false);
  CheckShillConfiguration(/*is_installed=*/false);

  ShillDeviceClient::Get()->GetTestInterface()->AddDevice(
      "/device/cellular1", shill::kTypeCellular, "TestCellular");
  FastForwardProfileRefreshDelay();
  FastForwardAutoConnectWaiting(/*auto_connect=*/false);
  base::RunLoop().RunUntilIdle();
  CheckShillConfiguration(/*is_installed=*/true);
}

TEST_F(CellularPolicyHandlerLegacyTest, InstallWaitForEuicc) {
  HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode());
  // Verify the configuration is created automatically after EUICC becomes
  // available.
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/false);
  CheckShillConfiguration(/*is_installed=*/false);
  SetupEuicc();
  FastForwardProfileRefreshDelay();
  FastForwardAutoConnectWaiting(/*auto_connect=*/false);
  base::RunLoop().RunUntilIdle();
  CheckShillConfiguration(/*is_installed=*/true);
  CheckIccidSmdpPairInPref(/*is_installed=*/true);
}

TEST_F(CellularPolicyHandlerLegacyTest, RetryInstallProfile) {
  SetupEuicc();

  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode());

  // Make the first installation attempt fail due to an user error which will
  // not result in a retry.
  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextInstallProfileFromActivationCodeResult(
          HermesResponseStatus::kErrorAlreadyDisabled);
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/false);

  ExpectedHistogramState expected_state = {
      .hermes_install_failed_initial_count = 1,
  };
  CheckHistogramState(expected_state);

  FastForwardBy(kInstallationRetryDelay + base::Minutes(5));

  CheckHistogramState(expected_state);

  // Make the second installation attempt fail due to an external error which
  // will retry after an initial delay of 1 day.
  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextInstallProfileFromActivationCodeResult(
          HermesResponseStatus::kErrorSendHttpsFailure);
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/false);

  expected_state.hermes_install_failed_initial_count++;
  CheckHistogramState(expected_state);

  FastForwardBy(kInstallationRetryDelay + base::Minutes(5));

  expected_state.success_retry_count++;
  CheckHistogramState(expected_state);

  // Make the third installation attempt fail due to an internal error which
  // will retry aftern an initial delay of 5 minutes with an exponential
  // backoff.
  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextInstallProfileFromActivationCodeResult(
          HermesResponseStatus::kErrorUnknown);
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/false);

  expected_state.hermes_install_failed_initial_count++;
  CheckHistogramState(expected_state);

  FastForwardBy(kInstallationRetryDelayExponential);

  expected_state.success_retry_count++;
  CheckHistogramState(expected_state);
}

TEST_F(CellularPolicyHandlerLegacyTest, InstallProfileFailure) {
  SetupEuicc();

  // Make the first installation attempt fail, resulting in an immediate retry
  // delay of |kInstallationRetryDelay|.
  HermesEuiccClient::Get()
      ->GetTestInterface()
      ->SetNextInstallProfileFromActivationCodeResult(
          HermesResponseStatus::kErrorSendHttpsFailure);

  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode());
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/false);

  ExpectedHistogramState expected_state = {
      .hermes_install_failed_initial_count = 1,
  };
  CheckHistogramState(expected_state);

  CheckShillConfiguration(/*is_installed=*/false);
  CheckIccidSmdpPairInPref(/*is_installed=*/false);

  // Fast forward by |kInstallationRetryDelay| to trigger a retry. We use some
  // buffer time since the retry mechanism doesn't happen synchronously.
  FastForwardBy(kInstallationRetryDelay + base::Minutes(5));

  expected_state.success_retry_count++;
  CheckHistogramState(expected_state);

  CheckShillConfiguration(/*is_installed=*/true);
  CheckIccidSmdpPairInPref(/*is_installed=*/true);
}

TEST_F(CellularPolicyHandlerLegacyTest, InstallOnSecondEUICC) {
  SetupEuicc();
  // Verify esim profile get installed successfully when installing policy
  // on the external EUICC.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCellularUseSecondEuicc);
  SetupEuicc2();
  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode());
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/true);
  CheckShillConfiguration(/*is_installed=*/true);
  CheckIccidSmdpPairInPref(/*is_installed=*/true);
}

TEST_F(CellularPolicyHandlerLegacyTest, InstallNoEUICCAvailable) {
  SetupEuicc();
  // Verify esim profile doesn't get installed when installing policy esim
  // with no available EUICC.
  HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
  base::RunLoop().RunUntilIdle();
  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode());
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/false);
  CheckShillConfiguration(/*is_installed=*/false);
  CheckIccidSmdpPairInPref(/*is_installed=*/false);
}

TEST_F(CellularPolicyHandlerLegacyTest, UpdateSMDPAddress) {
  SetupEuicc();
  // Verify that the first request should be invalidated when the second
  // request is queued.
  std::string policy = GenerateCellularPolicy("000");
  InstallESimPolicy(policy,
                    /*activation_code=*/"000",
                    /*expect_install_success=*/false);
  policy = GenerateCellularPolicy(HermesEuiccClient::Get()
                                      ->GetTestInterface()
                                      ->GenerateFakeActivationCode());

  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/true);
  CheckShillConfiguration(/*is_installed=*/true);
  CheckIccidSmdpPairInPref(/*is_installed=*/true);
}

TEST_F(CellularPolicyHandlerLegacyTest, InstallExistingESimProfileSuccess) {
  SetupEuicc();
  SetupESimProfile();

  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode(),
                             kICCID);
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/false);
  CheckShillConfiguration(/*is_installed=*/true);
  CheckIccidSmdpPairInPref(/*is_installed=*/true);
}

TEST_F(CellularPolicyHandlerLegacyTest, InstallExistingESimProfileFailure) {
  SetupEuicc();
  SetupESimProfile();
  ShillManagerClient::Get()->GetTestInterface()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kFailure);
  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode(),
                             kICCID);
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/false);
  FastForwardBy(kInstallationRetryDelay);
  CheckShillConfiguration(/*is_installed=*/false);
  CheckIccidSmdpPairInPref(/*is_installed=*/false);
}

TEST_F(CellularPolicyHandlerLegacyTest, NoInternetConnection) {
  SetupEuicc();
  auto* shill_service = ShillServiceClient::Get()->GetTestInterface();
  shill_service->ClearServices();
  base::RunLoop().RunUntilIdle();
  // Verify that when no internet connection, the installation will keep
  // retrying until the internet is available.
  const std::string policy =
      GenerateCellularPolicy(HermesEuiccClient::Get()
                                 ->GetTestInterface()
                                 ->GenerateFakeActivationCode());
  InstallESimPolicy(policy,
                    HermesEuiccClient::Get()
                        ->GetTestInterface()
                        ->GenerateFakeActivationCode(),
                    /*expect_install_success=*/false);
  // Fast forward 6 minutes since the first retry should happen in 5 minutes.
  FastForwardBy(base::Minutes(6));
  CheckShillConfiguration(/*is_installed=*/false);
  CheckIccidSmdpPairInPref(/*is_installed=*/false);
  // Verify that cellular type of internet connectivity should not trigger the
  // installation.
  shill_service->AddService(kUnmanagedCellularServicePath,
                            kUnmanagedCellularGuid, kUnmanagedCellularName,
                            shill::kTypeCellular, shill::kStateOnline,
                            /*visible=*/true);
  base::RunLoop().RunUntilIdle();
  // Fast forward 11 minutes since the first retry should happen in 10 minutes.
  FastForwardBy(base::Minutes(11));
  CheckShillConfiguration(/*is_installed=*/false);
  CheckIccidSmdpPairInPref(/*is_installed=*/false);
  // Verify that installation succeeds when a non-cellular type internet is
  // available.
  shill_service->AddService(kWifiServicePath, kWifiGuid, kWifiName,
                            shill::kTypeWifi, shill::kStateOnline,
                            /*visible=*/true);
  base::RunLoop().RunUntilIdle();
  // Fast forward 21 minutes since the first retry should happen in 20 minutes.
  FastForwardBy(base::Minutes(21));
  CheckShillConfiguration(/*is_installed=*/true);
  CheckIccidSmdpPairInPref(/*is_installed=*/true);
}

}  // namespace ash
