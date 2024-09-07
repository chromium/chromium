// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_metrics_logger.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/cellular_esim_profile.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler_impl.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/mock_managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kTestEuiccPath[] = "euicc_path";
const char kTestIccid[] = "iccid";
const char kTestProfileName[] = "test_profile_name";
const char kTestEidName[] = "eid";
const char kTestPSimGuid[] = "psim_guid";
const char kTestESimGuid[] = "esim_guid";
const char kTestESimPolicyGuid[] = "esim_policy_guid";

const char kTestCellularDevicePath[] = "/device/wwan0";
const char kTestPSimCellularServicePath[] = "/service/cellular0";
const char kTestESimCellularServicePath[] = "/service/cellular1";
const char kTestESimPolicyCellularServicePath[] = "/service/cellular2";
const char kTestEthServicePath[] = "/service/eth0";

const char kTestDomain[] = "test_domain";
const char kTestDeviceId[] = "test_device_id";

constexpr char kEnterpriseESimPolicy[] = R"({
  "NetworkConfigurations": [
    {
      "GUID": "cellular",
      "Type": "Cellular",
      "Name": "Test-Cellular",
      "Cellular": {}
    }
  ],
  "Type": "UnencryptedConfiguration"
})";

const char kESimFeatureUsageMetric[] = "ChromeOS.FeatureUsage.ESim";
const char kESimFeatureUsageUsetimeMetric[] =
    "ChromeOS.FeatureUsage.ESim.Usetime";
const char kEnterpriseESimFeatureUsageMetric[] =
    "ChromeOS.FeatureUsage.EnterpriseESim";
const char kEnterpriseESimFeatureUsageUsetimeMetric[] =
    "ChromeOS.FeatureUsage.EnterpriseESim.Usetime";

const char kPSimUsageCountHistogram[] = "Network.Cellular.PSim.Usage.Count";
const char kESimUsageCountHistogram[] = "Network.Cellular.ESim.Usage.Count";
const char kESimPolicyUsageCountHistogram[] =
    "Network.Cellular.ESim.Policy.Usage.Count";

const char kUnrestrictedPSimServiceAtLoginHistogram[] =
    "Network.Cellular.PSim.ServiceAtLoginCount.SimLockAllowedByPolicy";
const char kUnrestrictedESimServiceAtLoginHistogram[] =
    "Network.Cellular.ESim.ServiceAtLoginCount.SimLockAllowedByPolicy";
const char kRestrictedPSimServiceAtLoginHistogram[] =
    "Network.Cellular.PSim.ServiceAtLoginCount.SimLockProhibitedByPolicy";
const char kRestrictedESimServiceAtLoginHistogram[] =
    "Network.Cellular.ESim.ServiceAtLoginCount.SimLockProhibitedByPolicy";
const char kESimPolicyServiceAtLoginHistogram[] =
    "Network.Cellular.ESim.Policy.ServiceAtLogin.Count";

const char kPSimStatusAtLoginHistogram[] =
    "Network.Cellular.PSim.StatusAtLogin";
const char kESimStatusAtLoginHistogram[] =
    "Network.Cellular.ESim.StatusAtLogin";

const char kPSimTimeToConnectedHistogram[] =
    "Network.Cellular.PSim.TimeToConnected";
const char kESimTimeToConnectedHistogram[] =
    "Network.Cellular.ESim.TimeToConnected";

const char kPSimDisconnectionsHistogram[] =
    "Network.Cellular.PSim.Disconnections";
const char kESimDisconnectionsHistogram[] =
    "Network.Cellular.ESim.Disconnections";
const char kESimPolicyDisconnectionsHistogram[] =
    "Network.Cellular.ESim.Policy.Disconnections";

}  // namespace

class CellularMetricsLoggerTest : public ::testing::Test {
 public:
  CellularMetricsLoggerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  CellularMetricsLoggerTest(const CellularMetricsLoggerTest&) = delete;
  CellularMetricsLoggerTest& operator=(const CellularMetricsLoggerTest&) =
      delete;

  ~CellularMetricsLoggerTest() override = default;

  void SetUp() override {
    ResetHistogramTester();
    LoginState::Initialize();

    cellular_inhibitor_ = std::make_unique<CellularInhibitor>();
    mock_managed_network_configuration_handler_ = base::WrapUnique(
        new ::testing::NiceMock<MockManagedNetworkConfigurationHandler>);
    cellular_esim_profile_handler_ =
        std::make_unique<TestCellularESimProfileHandler>();
    network_config_helper_ =
        std::make_unique<network_config::CrosNetworkConfigTestHelper>();

    network_state_test_helper_.hermes_manager_test()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEidName, /*is_active=*/true,
        /*physical_slot=*/0);
    cellular_esim_profile_handler_->Init(
        network_state_test_helper_.network_state_handler(),
        cellular_inhibitor_.get());
    base::RunLoop().RunUntilIdle();
  }

  void InitMetricsLogger(bool check_esim_feature_eligible = true,
                         bool check_enterprise_esim_feature_eligible = false) {
    cellular_metrics_logger_.reset(new CellularMetricsLogger());
    cellular_metrics_logger_->Init(
        network_state_test_helper_.network_state_handler(),
        /*network_connection_handler=*/nullptr,
        cellular_esim_profile_handler_.get(),
        mock_managed_network_configuration_handler_.get());

    if (check_esim_feature_eligible) {
      histogram_tester_->ExpectTotalCount(kESimFeatureUsageMetric, 0);
    }
    if (check_enterprise_esim_feature_eligible &&
        InstallAttributes::IsInitialized() &&
        InstallAttributes::Get()->IsEnterpriseManaged()) {
      histogram_tester_->ExpectTotalCount(kEnterpriseESimFeatureUsageMetric, 0);
    }
    task_environment_.FastForwardBy(
        feature_usage::FeatureUsageMetrics::kInitialInterval);
    if (check_esim_feature_eligible) {
      histogram_tester_->ExpectBucketCount(
          kESimFeatureUsageMetric,
          static_cast<int>(
              feature_usage::FeatureUsageMetrics::Event::kEligible),
          1);
    }
    if (check_enterprise_esim_feature_eligible &&
        InstallAttributes::IsInitialized() &&
        InstallAttributes::Get()->IsEnterpriseManaged()) {
      histogram_tester_->ExpectBucketCount(
          kEnterpriseESimFeatureUsageMetric,
          static_cast<int>(
              feature_usage::FeatureUsageMetrics::Event::kEligible),
          1);
    }
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    network_config_helper_.reset();
    cellular_metrics_logger_.reset();
    LoginState::Shutdown();
  }

 protected:
  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void InitEthernet() {
    ShillServiceClient::TestInterface* service_test =
        network_state_test_helper_.service_test();
    service_test->AddService(kTestEthServicePath, "test_guid1", "test_eth",
                             shill::kTypeEthernet, shill::kStateIdle, true);
    base::RunLoop().RunUntilIdle();
  }

  void InitCellular() {
    ShillDeviceClient::TestInterface* device_test =
        network_state_test_helper_.device_test();
    device_test->AddDevice(kTestCellularDevicePath, shill::kTypeCellular,
                           "test_cellular_device");

    ShillServiceClient::TestInterface* service_test =
        network_state_test_helper_.service_test();
    service_test->AddService(kTestPSimCellularServicePath, kTestPSimGuid,
                             "test_cellular", shill::kTypeCellular,
                             shill::kStateIdle, true);
    service_test->AddService(kTestESimCellularServicePath, kTestESimGuid,
                             "test_cellular_2", shill::kTypeCellular,
                             shill::kStateIdle, true);
    service_test->AddService(kTestESimPolicyCellularServicePath,
                             kTestESimPolicyGuid, "test_cellular_3",
                             shill::kTypeCellular, shill::kStateIdle, true);
    base::RunLoop().RunUntilIdle();

    service_test->SetServiceProperty(
        kTestPSimCellularServicePath, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateNotActivated));
    service_test->SetServiceProperty(
        kTestESimCellularServicePath, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateNotActivated));
    service_test->SetServiceProperty(
        kTestESimPolicyCellularServicePath, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateNotActivated));
    service_test->SetServiceProperty(kTestESimCellularServicePath,
                                     shill::kEidProperty,
                                     base::Value("test_eid"));
    // Emulate 'test_cellular_3' being a managed network.
    service_test->SetServiceProperty(
        kTestESimPolicyCellularServicePath, shill::kONCSourceProperty,
        base::Value(shill::kONCSourceDevicePolicy));
    std::unique_ptr<NetworkUIData> ui_data = NetworkUIData::CreateFromONC(
        ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
    service_test->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                     shill::kUIDataProperty,
                                     base::Value(ui_data->GetAsJson()));
    service_test->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                     shill::kEidProperty,
                                     base::Value("test_eid_2"));

    base::RunLoop().RunUntilIdle();
  }

  void SetCellularSimLock(const std::string lock_type) {
    auto sim_lock_status =
        base::Value::Dict().Set(shill::kSIMLockTypeProperty, lock_type);
    network_config_helper_->network_state_helper()
        .device_test()
        ->SetDeviceProperty(
            kTestCellularDevicePath, shill::kSIMLockStatusProperty,
            base::Value(std::move(sim_lock_status)), /*notify_changed=*/true);
  }

  void RemoveCellular() {
    ShillServiceClient::TestInterface* service_test =
        network_state_test_helper_.service_test();
    service_test->RemoveService(kTestPSimCellularServicePath);

    ShillDeviceClient::TestInterface* device_test =
        network_state_test_helper_.device_test();
    device_test->RemoveDevice(kTestCellularDevicePath);
    base::RunLoop().RunUntilIdle();
  }

  void RemoveCellularService(const std::string& service_path) {
    ShillServiceClient::TestInterface* service_test =
        network_state_test_helper_.service_test();
    service_test->RemoveService(service_path);
    base::RunLoop().RunUntilIdle();
  }

  void AddESimProfile(hermes::profile::State state,
                      const std::string& service_path) {
    network_state_test_helper_.hermes_euicc_test()->AddCarrierProfile(
        dbus::ObjectPath(service_path), dbus::ObjectPath(kTestEuiccPath),
        kTestIccid, kTestProfileName, /*nickname=*/"", "service_provider",
        "activation_code", service_path, state,
        hermes::profile::ProfileClass::kOperational,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);
    base::RunLoop().RunUntilIdle();
  }

  void ClearEuicc() {
    network_state_test_helper_.hermes_euicc_test()->ClearEuicc(
        dbus::ObjectPath(kTestEuiccPath));
    base::RunLoop().RunUntilIdle();
  }

  void RemoveEuicc() {
    network_state_test_helper_.hermes_manager_test()->ClearEuiccs();
    base::RunLoop().RunUntilIdle();
  }

  void MarkEnterpriseEnrolled() {
    stub_install_attributes_.Get()->SetCloudManaged(kTestDomain, kTestDeviceId);
    base::RunLoop().RunUntilIdle();
  }

  void ResetLogin() {
    LoginState::Get()->SetLoggedInState(
        LoginState::LoggedInState::LOGGED_IN_NONE,
        LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);
    LoginState::Get()->SetLoggedInState(
        LoginState::LoggedInState::LOGGED_IN_ACTIVE,
        LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);
  }

  ShillServiceClient::TestInterface* service_client_test() {
    return network_state_test_helper_.service_test();
  }

  CellularMetricsLogger* cellular_metrics_logger() const {
    return cellular_metrics_logger_.get();
  }

  void ValidateServiceCount(size_t restricted_count,
                            size_t unrestricted_count) {
    histogram_tester_->ExpectTotalCount(kRestrictedESimServiceAtLoginHistogram,
                                        restricted_count);
    histogram_tester_->ExpectTotalCount(kRestrictedPSimServiceAtLoginHistogram,
                                        restricted_count);
    histogram_tester_->ExpectTotalCount(
        kUnrestrictedESimServiceAtLoginHistogram, unrestricted_count);
    histogram_tester_->ExpectTotalCount(
        kUnrestrictedPSimServiceAtLoginHistogram, unrestricted_count);
  }

  ash::ScopedStubInstallAttributes stub_install_attributes_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
  std::unique_ptr<network_config::CrosNetworkConfigTestHelper>
      network_config_helper_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
  std::unique_ptr<CellularMetricsLogger> cellular_metrics_logger_;
  std::unique_ptr<MockManagedNetworkConfigurationHandler>
      mock_managed_network_configuration_handler_;
};

TEST_F(CellularMetricsLoggerTest, NoEuiccCachedProfiles) {
  // Chrome caches eSIM profile information from Hermes so that this information
  // is available even when Hermes is not. Simulate the situation where Chrome
  // has eSIM information cached in prefs and Hermes being unavailable and
  // confirm that ESimFeatureUsageMetrics still behaves as expected. This is a
  // regression test for b/291812699.
  const CellularESimProfile esim_profile(
      CellularESimProfile::State::kActive, dbus::ObjectPath("profile_path"),
      std::string("eid"), std::string("iccid"), std::u16string(u"name"),
      std::u16string(u"nickname"), std::u16string(u"service_provider"),
      std::string("activation_code"));
  auto esim_profiles =
      base::Value::List().Append(esim_profile.ToDictionaryValue());

  TestingPrefServiceSimple device_prefs;
  CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(
      device_prefs.registry());
  device_prefs.Set(prefs::kESimProfiles, base::Value(std::move(esim_profiles)));

  ClearEuicc();

  InitMetricsLogger();

  histogram_tester_->ExpectTotalCount(kESimFeatureUsageMetric, 1);
  histogram_tester_->ExpectBucketCount(
      kESimFeatureUsageMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      1);
}

TEST_F(CellularMetricsLoggerTest, ActiveProfileExists) {
  InitCellular();
  AddESimProfile(hermes::profile::State::kActive, kTestESimCellularServicePath);
  InitMetricsLogger();
  histogram_tester_->ExpectTotalCount(kESimFeatureUsageMetric, 2);
  histogram_tester_->ExpectBucketCount(
      kESimFeatureUsageMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 1);
}

TEST_F(CellularMetricsLoggerTest, CellularServiceAtLoginWithRestrictedSimLock) {
  InitMetricsLogger();
  ON_CALL(*mock_managed_network_configuration_handler_, AllowCellularSimLock())
      .WillByDefault(::testing::Return(false));

  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);
  InitCellular();
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);

  ValidateServiceCount(1, 0);
}

TEST_F(CellularMetricsLoggerTest, CellularServiceAtLoginTest) {
  InitMetricsLogger();
  ON_CALL(*mock_managed_network_configuration_handler_, AllowCellularSimLock())
      .WillByDefault(::testing::Return(true));

  // Should defer logging when there are no cellular networks.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);
  ValidateServiceCount(0, 0);
  histogram_tester_->ExpectTotalCount(kESimPolicyServiceAtLoginHistogram, 0);

  // Should wait until initialization timeout before logging status.
  InitCellular();
  ValidateServiceCount(0, 0);
  histogram_tester_->ExpectTotalCount(kESimPolicyServiceAtLoginHistogram, 0);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  ValidateServiceCount(0, 1);
  histogram_tester_->ExpectTotalCount(kESimPolicyServiceAtLoginHistogram, 1);

  // Should log immediately when networks are already initialized.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_NONE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);
  ValidateServiceCount(0, 2);
  histogram_tester_->ExpectTotalCount(kESimPolicyServiceAtLoginHistogram, 2);

  // Should not log when the logged in user is neither owner nor regular.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_KIOSK);
  ValidateServiceCount(0, 2);
  histogram_tester_->ExpectTotalCount(kESimPolicyServiceAtLoginHistogram, 2);
}

TEST_F(CellularMetricsLoggerTest, AllowApnModificationTest) {
  InitMetricsLogger();
  ON_CALL(*mock_managed_network_configuration_handler_, AllowApnModification())
      .WillByDefault(::testing::Return(false));

  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_NONE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);

  histogram_tester_->ExpectBucketCount(
      "Network.Ash.Cellular.Apn.Login.AllowApnModification", false, 1);
}

TEST_F(CellularMetricsLoggerTest, CellularUsageCountTest) {
  InitMetricsLogger();

  InitEthernet();
  InitCellular();
  static const base::Value kTestOnlineStateValue(shill::kStateOnline);
  static const base::Value kTestIdleStateValue(shill::kStateIdle);

  AddESimProfile(hermes::profile::State::kActive, kTestESimCellularServicePath);

  // Should not log state until after timeout.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(kPSimUsageCountHistogram, 0);

  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 1);
  histogram_tester_->ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 1);

  // PSim Cellular connected with other network.
  service_client_test()->SetServiceProperty(kTestPSimCellularServicePath,
                                            shill::kStateProperty,
                                            kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);
  histogram_tester_->ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 0);

  histogram_tester_->ExpectBucketCount(
      kESimFeatureUsageMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      0);

  // PSim Cellular connected as only network.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);
  histogram_tester_->ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 0);

  histogram_tester_->ExpectBucketCount(
      kESimFeatureUsageMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      0);

  // After |kTimeSpentOnlinePSim|, PSim Cellular becomes not connected.
  const base::TimeDelta kTimeSpentOnlinePSim = base::Seconds(123);
  task_environment_.FastForwardBy(kTimeSpentOnlinePSim);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 2);
  histogram_tester_->ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 1);
  histogram_tester_->ExpectTimeBucketCount(
      "Network.Cellular.PSim.Usage.Duration", kTimeSpentOnlinePSim, 1);

  // Connect ethernet network.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestOnlineStateValue);

  // ESim Cellular connected.
  service_client_test()->SetServiceProperty(kTestESimCellularServicePath,
                                            shill::kStateProperty,
                                            kTestOnlineStateValue);

  // ESim Cellular connected with other network.
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);
  histogram_tester_->ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);

  // ESim Cellular connected as only network.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);
  histogram_tester_->ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);

  // After |kTimeSpentOnlineESim|, ESim Cellular becomes not connected.
  const base::TimeDelta kTimeSpentOnlineESim = base::Seconds(123);
  task_environment_.FastForwardBy(kTimeSpentOnlineESim);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 2);
  histogram_tester_->ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 2);
  histogram_tester_->ExpectTimeBucketCount(
      "Network.Cellular.ESim.Usage.Duration", kTimeSpentOnlineESim, 1);
  histogram_tester_->ExpectTimeBucketCount(kESimFeatureUsageUsetimeMetric,
                                           kTimeSpentOnlineESim, 1);

  // Connect ethernet network.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestOnlineStateValue);

  // ESim Policy Cellular connected.
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            kTestOnlineStateValue);
  // ESim Policy Cellular connected with other network.
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);
  histogram_tester_->ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 2);
  histogram_tester_->ExpectBucketCount(
      kESimPolicyUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);

  // ESim Policy Cellular connected as only network.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);
  histogram_tester_->ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 2);
  histogram_tester_->ExpectBucketCount(
      kESimPolicyUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);

  // After |kTimeSpentOnlineESim|, ESim Policy Cellular becomes not connected.
  task_environment_.FastForwardBy(kTimeSpentOnlineESim);
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();

  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 2);
  histogram_tester_->ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 3);
  histogram_tester_->ExpectBucketCount(
      kESimPolicyUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 1);
  histogram_tester_->ExpectTimeBucketCount(
      "Network.Cellular.ESim.Usage.Duration", kTimeSpentOnlineESim, 2);
  histogram_tester_->ExpectTimeBucketCount(
      "Network.Cellular.ESim.Policy.Usage.Duration", kTimeSpentOnlineESim, 1);
  histogram_tester_->ExpectTimeBucketCount(kESimFeatureUsageUsetimeMetric,
                                           kTimeSpentOnlineESim, 2);
}

TEST_F(CellularMetricsLoggerTest, CellularUsageCountDongleTest) {
  InitMetricsLogger();

  InitEthernet();
  static const base::Value kTestOnlineStateValue(shill::kStateOnline);
  static const base::Value kTestIdleStateValue(shill::kStateIdle);

  // Should not log state if no cellular devices are available.
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester_->ExpectTotalCount(kPSimUsageCountHistogram, 0);

  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(kPSimUsageCountHistogram, 0);

  // Should log state if a new cellular device is plugged in.
  InitCellular();
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 1);

  // Should log connected state for cellular device that was plugged in.
  service_client_test()->SetServiceProperty(kTestPSimCellularServicePath,
                                            shill::kStateProperty,
                                            kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);

  // Should log connected as only network state for cellular device
  // that was plugged in.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);

  // Should not log state if cellular device is unplugged.
  RemoveCellular();
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(kPSimUsageCountHistogram, 3);
}

TEST_F(CellularMetricsLoggerTest, CellularESimProfileStatusAtLoginTest) {
  InitMetricsLogger();

  // Should defer logging when there are no cellular networks.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);
  histogram_tester_->ExpectTotalCount(kESimStatusAtLoginHistogram, 0);

  // Should wait until initialization timeout before logging status.
  InitCellular();
  histogram_tester_->ExpectTotalCount(kESimStatusAtLoginHistogram, 0);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester_->ExpectBucketCount(
      kESimStatusAtLoginHistogram,
      CellularMetricsLogger::ESimProfileStatus::kNoProfiles, 1);

  ClearEuicc();
  AddESimProfile(hermes::profile::State::kActive, kTestPSimCellularServicePath);
  ResetLogin();
  histogram_tester_->ExpectBucketCount(
      kESimStatusAtLoginHistogram,
      CellularMetricsLogger::ESimProfileStatus::kActive, 1);

  ClearEuicc();
  AddESimProfile(hermes::profile::State::kInactive,
                 kTestPSimCellularServicePath);
  ResetLogin();
  histogram_tester_->ExpectBucketCount(
      kESimStatusAtLoginHistogram,
      CellularMetricsLogger::ESimProfileStatus::kActive, 2);

  ClearEuicc();
  AddESimProfile(hermes::profile::State::kActive, kTestPSimCellularServicePath);
  AddESimProfile(hermes::profile::State::kPending,
                 kTestESimCellularServicePath);
  ResetLogin();
  histogram_tester_->ExpectBucketCount(
      kESimStatusAtLoginHistogram,
      CellularMetricsLogger::ESimProfileStatus::kActiveWithPendingProfiles, 1);

  ClearEuicc();
  AddESimProfile(hermes::profile::State::kInactive,
                 kTestPSimCellularServicePath);
  AddESimProfile(hermes::profile::State::kPending,
                 kTestESimCellularServicePath);
  ResetLogin();
  histogram_tester_->ExpectBucketCount(
      kESimStatusAtLoginHistogram,
      CellularMetricsLogger::ESimProfileStatus::kActiveWithPendingProfiles, 2);

  ClearEuicc();
  AddESimProfile(hermes::profile::State::kPending,
                 kTestESimCellularServicePath);
  ResetLogin();
  histogram_tester_->ExpectBucketCount(
      kESimStatusAtLoginHistogram,
      CellularMetricsLogger::ESimProfileStatus::kPendingProfilesOnly, 1);
}

TEST_F(CellularMetricsLoggerTest, CellularPSimActivationStateAtLoginTest) {
  InitMetricsLogger();

  // Should defer logging when there are no cellular networks.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);
  histogram_tester_->ExpectTotalCount(kPSimStatusAtLoginHistogram, 0);

  // Should wait until initialization timeout before logging status.
  InitCellular();
  histogram_tester_->ExpectTotalCount(kPSimStatusAtLoginHistogram, 0);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester_->ExpectBucketCount(
      kPSimStatusAtLoginHistogram,
      CellularMetricsLogger::PSimActivationState::kNotActivated, 1);

  // Should log immediately when networks are already initialized.
  ResetLogin();
  histogram_tester_->ExpectBucketCount(
      kPSimStatusAtLoginHistogram,
      CellularMetricsLogger::PSimActivationState::kNotActivated, 2);

  // Should log activated state.
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  base::RunLoop().RunUntilIdle();
  ResetLogin();
  histogram_tester_->ExpectBucketCount(
      kPSimStatusAtLoginHistogram,
      CellularMetricsLogger::PSimActivationState::kActivated, 1);

  // Should not log when the logged in user is neither owner nor regular.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_KIOSK);
  histogram_tester_->ExpectTotalCount(kPSimStatusAtLoginHistogram, 3);
}

TEST_F(CellularMetricsLoggerTest, CellularConnectResult) {
  InitMetricsLogger();

  InitCellular();

  const base::Value kFailure(shill::kStateFailure);
  const base::Value kAssocStateValue(shill::kStateAssociation);
  ResetHistogramTester();

  AddESimProfile(hermes::profile::State::kActive, kTestESimCellularServicePath);

  // Set cellular networks to connecting state.
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kAssocStateValue);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kAssocStateValue);
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            kAssocStateValue);
  base::RunLoop().RunUntilIdle();

  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimAllConnectionResultHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimPolicyAllConnectionResultHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kPSimAllConnectionResultHistogram, 0);

  // Set cellular networks to failed state.
  service_client_test()->SetServiceProperty(kTestPSimCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateFailure));
  service_client_test()->SetServiceProperty(kTestESimCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateFailure));
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateFailure));
  base::RunLoop().RunUntilIdle();

  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimAllConnectionResultHistogram, 2);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimPolicyAllConnectionResultHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kPSimAllConnectionResultHistogram, 1);

  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kESimAllConnectionResultHistogram,
      CellularMetricsLogger::ShillConnectResult::kUnknown, 2);
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kESimPolicyAllConnectionResultHistogram,
      CellularMetricsLogger::ShillConnectResult::kUnknown, 1);
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kPSimAllConnectionResultHistogram,
      CellularMetricsLogger::ShillConnectResult::kUnknown, 1);

  histogram_tester_->ExpectBucketCount(
      kESimFeatureUsageMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithFailure),
      2);

  // Set cellular networks to connecting state.
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kAssocStateValue);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kAssocStateValue);
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            kAssocStateValue);
  base::RunLoop().RunUntilIdle();

  // Set cellular networks to connected state.
  service_client_test()->SetServiceProperty(kTestPSimCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateOnline));
  service_client_test()->SetServiceProperty(kTestESimCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateOnline));
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimAllConnectionResultHistogram, 4);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimPolicyAllConnectionResultHistogram, 2);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kPSimAllConnectionResultHistogram, 2);

  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kESimAllConnectionResultHistogram,
      CellularMetricsLogger::ShillConnectResult::kSuccess, 2);
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kESimPolicyAllConnectionResultHistogram,
      CellularMetricsLogger::ShillConnectResult::kSuccess, 1);
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kPSimAllConnectionResultHistogram,
      CellularMetricsLogger::ShillConnectResult::kSuccess, 1);

  histogram_tester_->ExpectBucketCount(
      kESimFeatureUsageMetric,
      static_cast<int>(
          feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess),
      1);

  // Set cellular networks to disconnected state.
  service_client_test()->SetServiceProperty(kTestPSimCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateIdle));
  service_client_test()->SetServiceProperty(kTestESimCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateIdle));
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();

  // A connected to disconnected state change should not impact connection
  // success.
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimAllConnectionResultHistogram, 4);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimPolicyAllConnectionResultHistogram, 2);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kPSimAllConnectionResultHistogram, 2);

  // Set cellular networks to connected state.
  service_client_test()->SetServiceProperty(kTestPSimCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateOnline));
  service_client_test()->SetServiceProperty(kTestESimCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateOnline));
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  // A disconnected to connected state, skipping the connecting state change
  // should emit a success.
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimAllConnectionResultHistogram, 6);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimPolicyAllConnectionResultHistogram, 3);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kPSimAllConnectionResultHistogram, 3);

  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kESimAllConnectionResultHistogram,
      CellularMetricsLogger::ShillConnectResult::kSuccess, 4);
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kESimPolicyAllConnectionResultHistogram,
      CellularMetricsLogger::ShillConnectResult::kSuccess, 2);
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kPSimAllConnectionResultHistogram,
      CellularMetricsLogger::ShillConnectResult::kSuccess, 2);

  // User initiated connection histograms should be 0 the entire time because
  // the previous connection state changes occured from Shill.
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimUserInitiatedConnectionResultHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kPSimUserInitiatedConnectionResultHistogram, 0);
}

TEST_F(CellularMetricsLoggerTest, CancellationDuringConnecting) {
  InitMetricsLogger();
  InitCellular();
  ResetHistogramTester();
  base::RunLoop().RunUntilIdle();

  AddESimProfile(hermes::profile::State::kActive, kTestESimCellularServicePath);

  // Set cellular networks to connecting state.
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty,
      base::Value(shill::kStateAssociation));
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty,
      base::Value(shill::kStateAssociation));
  service_client_test()->SetServiceProperty(
      kTestESimPolicyCellularServicePath, shill::kStateProperty,
      base::Value(shill::kStateAssociation));
  base::RunLoop().RunUntilIdle();

  // Simulate chrome disconnect request.
  cellular_metrics_logger()->DisconnectRequested(kTestESimCellularServicePath);
  cellular_metrics_logger()->DisconnectRequested(
      kTestESimPolicyCellularServicePath);
  cellular_metrics_logger()->DisconnectRequested(kTestPSimCellularServicePath);

  // Set cellular networks to failed state via shill.
  service_client_test()->SetServiceProperty(kTestPSimCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateIdle));
  service_client_test()->SetServiceProperty(kTestESimCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateIdle));
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();

  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimUserInitiatedConnectionResultHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimUserInitiatedConnectionResultHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimAllConnectionResultHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimPolicyAllConnectionResultHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kPSimAllConnectionResultHistogram, 0);

  // Set cellular networks to connecting state.
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty,
      base::Value(shill::kStateAssociation));
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty,
      base::Value(shill::kStateAssociation));
  service_client_test()->SetServiceProperty(
      kTestESimPolicyCellularServicePath, shill::kStateProperty,
      base::Value(shill::kStateAssociation));
  base::RunLoop().RunUntilIdle();

  // Set cellular networks to failed state via shill.
  service_client_test()->SetServiceProperty(kTestPSimCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateIdle));
  service_client_test()->SetServiceProperty(kTestESimCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateIdle));
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();

  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimUserInitiatedConnectionResultHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimUserInitiatedConnectionResultHistogram, 0);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimAllConnectionResultHistogram, 2);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kESimPolicyAllConnectionResultHistogram, 1);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kPSimAllConnectionResultHistogram, 1);
}

TEST_F(CellularMetricsLoggerTest, UserInitiatedConnectionResult) {
  InitMetricsLogger();
  InitCellular();
  ResetHistogramTester();
  base::RunLoop().RunUntilIdle();

  // Simulate chrome connect success
  cellular_metrics_logger()->ConnectSucceeded(kTestPSimCellularServicePath);
  cellular_metrics_logger()->ConnectSucceeded(kTestESimCellularServicePath);
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kESimUserInitiatedConnectionResultHistogram,
      CellularMetricsLogger::ConnectResult::kSuccess, 1);
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kPSimUserInitiatedConnectionResultHistogram,
      CellularMetricsLogger::ConnectResult::kSuccess, 1);

  // Simulate chrome connect failure
  cellular_metrics_logger()->ConnectFailed(
      kTestESimCellularServicePath,
      NetworkConnectionHandler::kErrorConnectCanceled);
  cellular_metrics_logger()->ConnectFailed(
      kTestPSimCellularServicePath,
      NetworkConnectionHandler::kErrorConnectCanceled);

  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kESimUserInitiatedConnectionResultHistogram,
      CellularMetricsLogger::ConnectResult::kCanceled, 1);
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kPSimUserInitiatedConnectionResultHistogram,
      CellularMetricsLogger::ConnectResult::kCanceled, 1);
}

TEST_F(CellularMetricsLoggerTest, SwitchActiveNetworkOnManagedDevice) {
  InitMetricsLogger();

  InitCellular();
  const base::Value kOnlineStateValue(shill::kStateOnline);
  const base::Value kIdleStateValue(shill::kStateIdle);
  const base::Value kFailedToConnect(shill::kStateFailure);

  ON_CALL(*mock_managed_network_configuration_handler_, AllowCellularSimLock())
      .WillByDefault(::testing::Return(false));

  ResetHistogramTester();
  AddESimProfile(hermes::profile::State::kActive, kTestESimCellularServicePath);

  // No connection -> Unlocked ESIM connection
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kRestrictedActiveNetworkSIMLockStatus, 1);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kUnrestrictedActiveNetworkSIMLockStatus, 0);
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kRestrictedActiveNetworkSIMLockStatus,
      CellularMetricsLogger::SimPinLockType::kUnlocked, 1);

  // Unlocked ESIM connection -> PSIM connection -> PinLocked ESiM connection
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();

  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  SetCellularSimLock(shill::kSIMLockPin);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kFailedToConnect);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kRestrictedActiveNetworkSIMLockStatus,
      CellularMetricsLogger::SimPinLockType::kPinLocked, 1);

  // PinLocked ESIM -> PSIM connection -> PukBlocked ESIM
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();

  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  SetCellularSimLock(shill::kSIMLockPuk);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kFailedToConnect);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kRestrictedActiveNetworkSIMLockStatus,
      CellularMetricsLogger::SimPinLockType::kPukLocked, 1);

  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  SetCellularSimLock(shill::kSIMLockNetworkPin);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kFailedToConnect);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      CellularMetricsLogger::kRestrictedActiveNetworkSIMLockStatus,
      CellularMetricsLogger::SimPinLockType::kCarrierLocked, 1);
}

TEST_F(CellularMetricsLoggerTest, SwitchActiveNetworkOnUnmanagedDevice) {
  InitMetricsLogger();

  InitCellular();
  const base::Value kOnlineStateValue(shill::kStateOnline);
  const base::Value kIdleStateValue(shill::kStateIdle);

  ON_CALL(*mock_managed_network_configuration_handler_, AllowCellularSimLock())
      .WillByDefault(::testing::Return(true));

  ResetHistogramTester();
  AddESimProfile(hermes::profile::State::kActive, kTestESimCellularServicePath);

  // Two activated SIMs, but none connected.
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kUnrestrictedActiveNetworkSIMLockStatus, 0);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kRestrictedActiveNetworkSIMLockStatus, 0);

  // No connection -> ESIM connection
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kUnrestrictedActiveNetworkSIMLockStatus, 1);
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kRestrictedActiveNetworkSIMLockStatus, 0);

  // ESIM connection -> PSIM connection
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kUnrestrictedActiveNetworkSIMLockStatus, 2);

  // PSIM connection -> No connection
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kUnrestrictedActiveNetworkSIMLockStatus, 2);

  // No connection -> PSIM connection
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(
      CellularMetricsLogger::kUnrestrictedActiveNetworkSIMLockStatus, 2);
}

TEST_F(CellularMetricsLoggerTest, CellularTimeToConnectedTest) {
  InitMetricsLogger();

  constexpr base::TimeDelta kTestConnectionTime = base::Milliseconds(321);
  InitCellular();
  const base::Value kOnlineStateValue(shill::kStateOnline);
  const base::Value kAssocStateValue(shill::kStateAssociation);

  AddESimProfile(hermes::profile::State::kActive, kTestESimCellularServicePath);

  // Should not log connection time when not activated.
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kAssocStateValue);
  base::RunLoop().RunUntilIdle();
  task_environment_.FastForwardBy(kTestConnectionTime);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(kPSimTimeToConnectedHistogram, 0);

  // Set cellular networks to activated state and connecting state.
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kAssocStateValue);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kAssocStateValue);
  base::RunLoop().RunUntilIdle();

  // Should log first network's connection time independently.
  task_environment_.FastForwardBy(kTestConnectionTime);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTimeBucketCount(kPSimTimeToConnectedHistogram,
                                           kTestConnectionTime, 1);

  // Should log second network's connection time independently.
  task_environment_.FastForwardBy(kTestConnectionTime);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTimeBucketCount(kESimTimeToConnectedHistogram,
                                           2 * kTestConnectionTime, 1);
}

TEST_F(CellularMetricsLoggerTest, CellularDisconnectionsTest) {
  InitMetricsLogger();

  InitCellular();
  base::Value kOnlineStateValue(shill::kStateOnline);
  base::Value kIdleStateValue(shill::kStateIdle);

  AddESimProfile(hermes::profile::State::kActive, kTestESimCellularServicePath);

  // Should log connected state.
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            kOnlineStateValue);

  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kPSimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kConnected, 1);
  histogram_tester_->ExpectBucketCount(
      kESimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kConnected, 2);
  histogram_tester_->ExpectBucketCount(
      kESimPolicyDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kConnected, 1);

  // Should not log user initiated disconnections.
  cellular_metrics_logger()->DisconnectRequested(kTestPSimCellularServicePath);
  cellular_metrics_logger()->DisconnectRequested(kTestESimCellularServicePath);
  cellular_metrics_logger()->DisconnectRequested(
      kTestESimPolicyCellularServicePath);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kDisconnectRequestTimeout / 2);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            kIdleStateValue);
  base::RunLoop().RunUntilIdle();

  histogram_tester_->ExpectBucketCount(
      kPSimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 0);
  histogram_tester_->ExpectBucketCount(
      kESimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 0);
  histogram_tester_->ExpectBucketCount(
      kESimPolicyDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 0);

  // Should log non user initiated disconnects.
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            kOnlineStateValue);
  base::RunLoop().RunUntilIdle();

  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            kIdleStateValue);
  base::RunLoop().RunUntilIdle();

  histogram_tester_->ExpectBucketCount(
      kPSimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 1);
  histogram_tester_->ExpectBucketCount(
      kESimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 2);
  histogram_tester_->ExpectBucketCount(
      kESimPolicyDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 1);

  // Should log non user initiated disconnects when a previous
  // disconnect request timed out.
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            kOnlineStateValue);
  base::RunLoop().RunUntilIdle();

  cellular_metrics_logger()->DisconnectRequested(kTestPSimCellularServicePath);
  cellular_metrics_logger()->DisconnectRequested(kTestESimCellularServicePath);
  cellular_metrics_logger()->DisconnectRequested(
      kTestESimPolicyCellularServicePath);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kDisconnectRequestTimeout * 2);
  service_client_test()->SetServiceProperty(
      kTestPSimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(
      kTestESimCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            kIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectBucketCount(
      kPSimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 2);
  histogram_tester_->ExpectBucketCount(
      kESimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 4);
  histogram_tester_->ExpectBucketCount(
      kESimPolicyDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 2);
}

TEST_F(CellularMetricsLoggerTest,
       EnterpriseESimFeatureUsageMetrics_NotEnrolled) {

  TestingPrefServiceSimple device_prefs;
  CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(
      device_prefs.registry());

  // Any cellular service that is considered enterprise enrolled will result in
  // enterprise eSIM feature usage being considered enabled.
  RemoveCellularService(kTestESimPolicyCellularServicePath);

  InitMetricsLogger(/*check_esim_feature_eligible=*/false,
                    /*check_enterprise_esim_feature_eligible=*/false);

  histogram_tester_->ExpectTotalCount(kEnterpriseESimFeatureUsageMetric, 0);
}

TEST_F(CellularMetricsLoggerTest,
       EnterpriseESimFeatureUsageMetrics_NotEligible) {

  MarkEnterpriseEnrolled();

  TestingPrefServiceSimple device_prefs;
  CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(
      device_prefs.registry());

  RemoveEuicc();
  RemoveCellularService(kTestESimPolicyCellularServicePath);

  InitMetricsLogger(/*check_esim_feature_eligible=*/false,
                    /*check_enterprise_esim_feature_eligible=*/false);

  histogram_tester_->ExpectTotalCount(kEnterpriseESimFeatureUsageMetric, 0);
}

TEST_F(CellularMetricsLoggerTest,
       EnterpriseESimFeatureUsageMetrics_EligibleViaEuicc) {

  MarkEnterpriseEnrolled();

  TestingPrefServiceSimple device_prefs;
  CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(
      device_prefs.registry());

  InitCellular();

  // Explicitly remove the managed cellular service.
  RemoveCellularService(kTestESimPolicyCellularServicePath);

  InitMetricsLogger(/*check_esim_feature_eligible=*/false,
                    /*check_enterprise_esim_feature_eligible=*/true);

  histogram_tester_->ExpectTotalCount(kEnterpriseESimFeatureUsageMetric, 1);
  histogram_tester_->ExpectBucketCount(
      kEnterpriseESimFeatureUsageMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      1);
  histogram_tester_->ExpectBucketCount(
      kEnterpriseESimFeatureUsageMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kAccessible),
      0);
}

TEST_F(CellularMetricsLoggerTest,
       EnterpriseESimFeatureUsageMetrics_Accessible) {

  MarkEnterpriseEnrolled();

  TestingPrefServiceSimple device_prefs;
  CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(
      device_prefs.registry());

  const std::optional<base::Value::Dict> policy =
      base::JSONReader::ReadDict(kEnterpriseESimPolicy);
  ASSERT_TRUE(policy.has_value());

  EXPECT_CALL(*mock_managed_network_configuration_handler_,
              GetGlobalConfigFromPolicy(::testing::_))
      .WillRepeatedly(::testing::Return(&policy.value()));

  InitCellular();

  // Explicitly remove the managed cellular service.
  RemoveCellularService(kTestESimPolicyCellularServicePath);

  InitMetricsLogger(/*check_esim_feature_eligible=*/false,
                    /*check_enterprise_esim_feature_eligible=*/true);

  histogram_tester_->ExpectTotalCount(kEnterpriseESimFeatureUsageMetric, 2);
  histogram_tester_->ExpectBucketCount(
      kEnterpriseESimFeatureUsageMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible),
      1);
  histogram_tester_->ExpectBucketCount(
      kEnterpriseESimFeatureUsageMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kAccessible),
      1);
  histogram_tester_->ExpectBucketCount(
      kEnterpriseESimFeatureUsageMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 0);
}

TEST_F(CellularMetricsLoggerTest,
       EnterpriseESimFeatureUsageMetrics_EnabledViaService) {

  MarkEnterpriseEnrolled();

  TestingPrefServiceSimple device_prefs;
  CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(
      device_prefs.registry());

  InitCellular();

  InitMetricsLogger(/*check_esim_feature_eligible=*/false,
                    /*check_enterprise_esim_feature_eligible=*/true);

  histogram_tester_->ExpectTotalCount(kEnterpriseESimFeatureUsageMetric, 3);
  histogram_tester_->ExpectBucketCount(
      kEnterpriseESimFeatureUsageMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 1);
}

TEST_F(CellularMetricsLoggerTest,
       EnterpriseESimFeatureUsageMetrics_EnabledAndUsage) {

  MarkEnterpriseEnrolled();

  TestingPrefServiceSimple device_prefs;
  CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(
      device_prefs.registry());

  const std::optional<base::Value::Dict> policy =
      base::JSONReader::ReadDict(kEnterpriseESimPolicy);
  ASSERT_TRUE(policy.has_value());

  EXPECT_CALL(*mock_managed_network_configuration_handler_,
              GetGlobalConfigFromPolicy(::testing::_))
      .WillRepeatedly(::testing::Return(&policy.value()));

  InitCellular();

  AddESimProfile(hermes::profile::State::kActive,
                 kTestESimPolicyCellularServicePath);

  InitMetricsLogger(/*check_esim_feature_eligible=*/false,
                    /*check_enterprise_esim_feature_eligible=*/true);

  histogram_tester_->ExpectTotalCount(kEnterpriseESimFeatureUsageMetric, 3);
  histogram_tester_->ExpectBucketCount(
      kEnterpriseESimFeatureUsageMetric,
      static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled), 1);

  // Connect to the enterprise eSIM service for 10 minutes and check that the
  // correct usage duration is reported.
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  const base::TimeDelta kTimeSpentOnline = base::Minutes(10);
  task_environment_.FastForwardBy(kTimeSpentOnline);
  service_client_test()->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();

  histogram_tester_->ExpectTotalCount(kEnterpriseESimFeatureUsageMetric, 4);
  histogram_tester_->ExpectTimeBucketCount(
      kEnterpriseESimFeatureUsageUsetimeMetric, kTimeSpentOnline, 1);
}

}  // namespace ash
