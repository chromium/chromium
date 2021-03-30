// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_metrics_logger.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/cellular_esim_profile.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/test_cellular_esim_profile_handler.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kTestEuiccPath[] = "euicc_path";
const char kTestIccid[] = "iccid";
const char kTestProfileName[] = "test_profile_name";
const char kTestEidName[] = "eid";

const char kTestCellularDevicePath[] = "/device/wwan0";
const char kTestCellularServicePath[] = "/service/cellular0";
const char kTestCellularServicePath2[] = "/service/cellular1";
const char kTestEthServicePath[] = "/service/eth0";

const char kPSimUsageCountHistogram[] = "Network.Cellular.PSim.Usage.Count";
const char kESimUsageCountHistogram[] = "Network.Cellular.ESim.Usage.Count";

const char kPSimServiceAtLoginHistogram[] =
    "Network.Cellular.PSim.ServiceAtLogin.Count";
const char kESimServiceAtLoginHistogram[] =
    "Network.Cellular.ESim.ServiceAtLogin.Count";

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

const char kPSimConnectionSuccessHistogram[] =
    "Network.Cellular.PSim.ConnectionSuccess";
const char kESimConnectionSuccessHistogram[] =
    "Network.Cellular.ESim.ConnectionSuccess";

}  // namespace

class CellularMetricsLoggerTest : public testing::Test {
 public:
  CellularMetricsLoggerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~CellularMetricsLoggerTest() override = default;

  void SetUp() override {
    LoginState::Initialize();

    cellular_inhibitor_ = std::make_unique<CellularInhibitor>();
    cellular_esim_profile_handler_ =
        std::make_unique<TestCellularESimProfileHandler>();

    cellular_metrics_logger_.reset(new CellularMetricsLogger());
    cellular_metrics_logger_->Init(
        network_state_test_helper_.network_state_handler(),
        /* network_connection_handler */ nullptr,
        cellular_esim_profile_handler_.get());
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    cellular_metrics_logger_.reset();
    LoginState::Shutdown();
  }

 protected:
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
    service_test->AddService(kTestCellularServicePath, "test_guid0",
                             "test_cellular", shill::kTypeCellular,
                             shill::kStateIdle, true);
    service_test->AddService(kTestCellularServicePath2, "test_guid1",
                             "test_cellular_2", shill::kTypeCellular,
                             shill::kStateIdle, true);
    base::RunLoop().RunUntilIdle();

    service_test->SetServiceProperty(
        kTestCellularServicePath, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateNotActivated));
    service_test->SetServiceProperty(
        kTestCellularServicePath2, shill::kActivationStateProperty,
        base::Value(shill::kActivationStateNotActivated));

    service_test->SetServiceProperty(kTestCellularServicePath2,
                                     shill::kEidProperty,
                                     base::Value("test_eid"));
    base::RunLoop().RunUntilIdle();

    network_state_test_helper_.hermes_manager_test()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEidName, /*is_active=*/true,
        /*physical_slot=*/0);
    cellular_esim_profile_handler_->Init(cellular_inhibitor_.get());
    base::RunLoop().RunUntilIdle();
  }

  void RemoveCellular() {
    ShillServiceClient::TestInterface* service_test =
        network_state_test_helper_.service_test();
    service_test->RemoveService(kTestCellularServicePath);

    ShillDeviceClient::TestInterface* device_test =
        network_state_test_helper_.device_test();
    device_test->RemoveDevice(kTestCellularDevicePath);
    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;

  ShillServiceClient::TestInterface* service_client_test() {
    return network_state_test_helper_.service_test();
  }

  CellularMetricsLogger* cellular_metrics_logger() const {
    return cellular_metrics_logger_.get();
  }

  void AddESimProfile(hermes::profile::State state,
                      const std::string& service_path) {
    network_state_test_helper_.hermes_euicc_test()->AddCarrierProfile(
        dbus::ObjectPath(service_path), dbus::ObjectPath(kTestEuiccPath),
        kTestIccid, kTestProfileName, "service_provider", "activation_code",
        service_path, state,
        /*service_only=*/false);
    base::RunLoop().RunUntilIdle();
  }

  void ClearEuicc() {
    network_state_test_helper_.hermes_euicc_test()->ClearEuicc(
        dbus::ObjectPath(kTestEuiccPath));
    base::RunLoop().RunUntilIdle();
  }

  void ResetLogin() {
    LoginState::Get()->SetLoggedInState(
        LoginState::LoggedInState::LOGGED_IN_NONE,
        LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);
    LoginState::Get()->SetLoggedInState(
        LoginState::LoggedInState::LOGGED_IN_ACTIVE,
        LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  }

 private:
  NetworkStateTestHelper network_state_test_helper_{
      false /* use_default_devices_and_services */};
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
  std::unique_ptr<CellularMetricsLogger> cellular_metrics_logger_;
  DISALLOW_COPY_AND_ASSIGN(CellularMetricsLoggerTest);
};

TEST_F(CellularMetricsLoggerTest, CellularServiceAtLoginTest) {
  base::HistogramTester histogram_tester;

  // Should defer logging when there are no cellular networks.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  histogram_tester.ExpectTotalCount(kESimServiceAtLoginHistogram, 0);
  histogram_tester.ExpectTotalCount(kPSimServiceAtLoginHistogram, 0);

  // Should wait until initialization timeout before logging status.
  InitCellular();
  histogram_tester.ExpectTotalCount(kESimServiceAtLoginHistogram, 0);
  histogram_tester.ExpectTotalCount(kPSimServiceAtLoginHistogram, 0);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectTotalCount(kESimServiceAtLoginHistogram, 1);
  histogram_tester.ExpectTotalCount(kPSimServiceAtLoginHistogram, 1);

  // Should log immediately when networks are already initialized.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_NONE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  histogram_tester.ExpectTotalCount(kESimServiceAtLoginHistogram, 2);
  histogram_tester.ExpectTotalCount(kPSimServiceAtLoginHistogram, 2);

  // Should not log when the logged in user is neither owner nor regular.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_KIOSK_APP);
  histogram_tester.ExpectTotalCount(kESimServiceAtLoginHistogram, 2);
  histogram_tester.ExpectTotalCount(kPSimServiceAtLoginHistogram, 2);
}

TEST_F(CellularMetricsLoggerTest, CellularUsageCountTest) {
  InitEthernet();
  InitCellular();
  base::HistogramTester histogram_tester;
  static const base::Value kTestOnlineStateValue(shill::kStateOnline);
  static const base::Value kTestIdleStateValue(shill::kStateIdle);

  // Should not log state until after timeout.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kPSimUsageCountHistogram, 0);

  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 1);
  histogram_tester.ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 1);

  // PSim Cellular connected with other network.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);
  histogram_tester.ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 0);

  // PSim Cellular connected as only network.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);
  histogram_tester.ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 0);

  // After |time_spent_online_psim|, PSim Cellular becomes not connected.
  const base::TimeDelta time_spent_online_psim =
      base::TimeDelta::FromSeconds(123);
  task_environment_.FastForwardBy(time_spent_online_psim);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 2);
  histogram_tester.ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 1);
  histogram_tester.ExpectTimeBucketCount("Network.Cellular.PSim.Usage.Duration",
                                         time_spent_online_psim, 1);

  // Connect ethernet network.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestOnlineStateValue);

  // ESim Cellular connected.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kStateProperty, kTestOnlineStateValue);

  // ESim Cellular connected with other network.
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);
  histogram_tester.ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);

  // ESim Cellular connected as only network.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);
  histogram_tester.ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);

  // After |time_spent_online_esim|, ESim Cellular becomes not connected.
  const base::TimeDelta time_spent_online_esim =
      base::TimeDelta::FromSeconds(321);
  task_environment_.FastForwardBy(time_spent_online_esim);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 2);
  histogram_tester.ExpectBucketCount(
      kESimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 2);
  histogram_tester.ExpectTimeBucketCount("Network.Cellular.ESim.Usage.Duration",
                                         time_spent_online_esim, 1);
}

TEST_F(CellularMetricsLoggerTest, CellularUsageCountDongleTest) {
  InitEthernet();
  base::HistogramTester histogram_tester;
  static const base::Value kTestOnlineStateValue(shill::kStateOnline);
  static const base::Value kTestIdleStateValue(shill::kStateIdle);

  // Should not log state if no cellular devices are available.
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectTotalCount(kPSimUsageCountHistogram, 0);

  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kPSimUsageCountHistogram, 0);

  // Should log state if a new cellular device is plugged in.
  InitCellular();
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kNotConnected, 1);

  // Should log connected state for cellular device that was plugged in.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);

  // Should log connected as only network state for cellular device
  // that was plugged in.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kPSimUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);

  // Should not log state if cellular device is unplugged.
  RemoveCellular();
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kPSimUsageCountHistogram, 3);
}

TEST_F(CellularMetricsLoggerTest, CellularESimProfileStatusAtLoginTest) {
  base::HistogramTester histogram_tester;

  // Should defer logging when there are no cellular networks.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  histogram_tester.ExpectTotalCount(kESimStatusAtLoginHistogram, 0);

  // Should wait until initialization timeout before logging status.
  InitCellular();
  histogram_tester.ExpectTotalCount(kESimStatusAtLoginHistogram, 0);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectBucketCount(
      kESimStatusAtLoginHistogram,
      CellularMetricsLogger::ESimProfileStatus::kNoProfiles, 1);

  ClearEuicc();
  AddESimProfile(hermes::profile::State::kActive, kTestCellularServicePath);
  ResetLogin();
  histogram_tester.ExpectBucketCount(
      kESimStatusAtLoginHistogram,
      CellularMetricsLogger::ESimProfileStatus::kActive, 1);

  ClearEuicc();
  AddESimProfile(hermes::profile::State::kInactive, kTestCellularServicePath);
  ResetLogin();
  histogram_tester.ExpectBucketCount(
      kESimStatusAtLoginHistogram,
      CellularMetricsLogger::ESimProfileStatus::kActive, 2);

  ClearEuicc();
  AddESimProfile(hermes::profile::State::kActive, kTestCellularServicePath);
  AddESimProfile(hermes::profile::State::kPending, kTestCellularServicePath2);
  ResetLogin();
  histogram_tester.ExpectBucketCount(
      kESimStatusAtLoginHistogram,
      CellularMetricsLogger::ESimProfileStatus::kActiveWithPendingProfiles, 1);

  ClearEuicc();
  AddESimProfile(hermes::profile::State::kInactive, kTestCellularServicePath);
  AddESimProfile(hermes::profile::State::kPending, kTestCellularServicePath2);
  ResetLogin();
  histogram_tester.ExpectBucketCount(
      kESimStatusAtLoginHistogram,
      CellularMetricsLogger::ESimProfileStatus::kActiveWithPendingProfiles, 2);

  ClearEuicc();
  AddESimProfile(hermes::profile::State::kPending, kTestCellularServicePath2);
  ResetLogin();
  histogram_tester.ExpectBucketCount(
      kESimStatusAtLoginHistogram,
      CellularMetricsLogger::ESimProfileStatus::kPendingProfilesOnly, 1);
}

TEST_F(CellularMetricsLoggerTest, CellularPSimActivationStateAtLoginTest) {
  base::HistogramTester histogram_tester;

  // Should defer logging when there are no cellular networks.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  histogram_tester.ExpectTotalCount(kPSimStatusAtLoginHistogram, 0);

  // Should wait until initialization timeout before logging status.
  InitCellular();
  histogram_tester.ExpectTotalCount(kPSimStatusAtLoginHistogram, 0);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectBucketCount(
      kPSimStatusAtLoginHistogram,
      CellularMetricsLogger::PSimActivationState::kNotActivated, 1);

  // Should log immediately when networks are already initialized.
  ResetLogin();
  histogram_tester.ExpectBucketCount(
      kPSimStatusAtLoginHistogram,
      CellularMetricsLogger::PSimActivationState::kNotActivated, 2);

  // Should log activated state.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  base::RunLoop().RunUntilIdle();
  ResetLogin();
  histogram_tester.ExpectBucketCount(
      kPSimStatusAtLoginHistogram,
      CellularMetricsLogger::PSimActivationState::kActivated, 1);

  // Should not log when the logged in user is neither owner nor regular.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_KIOSK_APP);
  histogram_tester.ExpectTotalCount(kPSimStatusAtLoginHistogram, 3);
}

TEST_F(CellularMetricsLoggerTest, CellularConnectResult) {
  InitCellular();
  base::HistogramTester histogram_tester;
  const base::Value kFailure(shill::kStateFailure);
  const base::Value kAssocStateValue(shill::kStateAssociation);

  // Set cellular networks to connecting state.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kAssocStateValue);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kStateProperty, kAssocStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kESimConnectionSuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(kPSimConnectionSuccessHistogram, 0);

  // Set cellular networks to failed state.
  service_client_test()->SetServiceProperty(kTestCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateFailure));
  service_client_test()->SetServiceProperty(kTestCellularServicePath2,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateFailure));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kESimConnectionSuccessHistogram, 1);
  histogram_tester.ExpectTotalCount(kPSimConnectionSuccessHistogram, 1);

  histogram_tester.ExpectBucketCount(
      kESimConnectionSuccessHistogram,
      CellularMetricsLogger::ConnectResult::kUnknown, 1);
  histogram_tester.ExpectBucketCount(
      kPSimConnectionSuccessHistogram,
      CellularMetricsLogger::ConnectResult::kUnknown, 1);

  // Set cellular networks to connecting state.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kAssocStateValue);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kStateProperty, kAssocStateValue);
  base::RunLoop().RunUntilIdle();

  // Set cellular networks to connected state.
  service_client_test()->SetServiceProperty(kTestCellularServicePath,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateOnline));
  service_client_test()->SetServiceProperty(kTestCellularServicePath2,
                                            shill::kStateProperty,
                                            base::Value(shill::kStateOnline));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(kESimConnectionSuccessHistogram, 2);
  histogram_tester.ExpectTotalCount(kPSimConnectionSuccessHistogram, 2);

  histogram_tester.ExpectBucketCount(
      kESimConnectionSuccessHistogram,
      CellularMetricsLogger::ConnectResult::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      kPSimConnectionSuccessHistogram,
      CellularMetricsLogger::ConnectResult::kSuccess, 1);

  // Simulate chrome connect failure
  cellular_metrics_logger()->ConnectFailed(
      kTestCellularServicePath,
      NetworkConnectionHandler::kErrorConnectCanceled);
  cellular_metrics_logger()->ConnectFailed(
      kTestCellularServicePath2,
      NetworkConnectionHandler::kErrorConnectCanceled);

  histogram_tester.ExpectTotalCount(kESimConnectionSuccessHistogram, 3);
  histogram_tester.ExpectTotalCount(kPSimConnectionSuccessHistogram, 3);

  histogram_tester.ExpectBucketCount(
      kESimConnectionSuccessHistogram,
      CellularMetricsLogger::ConnectResult::kCanceled, 1);
  histogram_tester.ExpectBucketCount(
      kPSimConnectionSuccessHistogram,
      CellularMetricsLogger::ConnectResult::kCanceled, 1);
}

TEST_F(CellularMetricsLoggerTest, CellularTimeToConnectedTest) {
  constexpr base::TimeDelta kTestConnectionTime =
      base::TimeDelta::FromMilliseconds(321);
  InitCellular();
  base::HistogramTester histogram_tester;
  const base::Value kOnlineStateValue(shill::kStateOnline);
  const base::Value kAssocStateValue(shill::kStateAssociation);

  // Should not log connection time when not activated.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kAssocStateValue);
  base::RunLoop().RunUntilIdle();
  task_environment_.FastForwardBy(kTestConnectionTime);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kPSimTimeToConnectedHistogram, 0);

  // Set cellular networks to activated state and connecting state.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kAssocStateValue);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kStateProperty, kAssocStateValue);
  base::RunLoop().RunUntilIdle();

  // Should log first network's connection time independently.
  task_environment_.FastForwardBy(kTestConnectionTime);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTimeBucketCount(kPSimTimeToConnectedHistogram,
                                         kTestConnectionTime, 1);

  // Should log second network's connection time independently.
  task_environment_.FastForwardBy(kTestConnectionTime);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTimeBucketCount(kESimTimeToConnectedHistogram,
                                         2 * kTestConnectionTime, 1);
}

TEST_F(CellularMetricsLoggerTest, CellularDisconnectionsTest) {
  InitCellular();
  base::HistogramTester histogram_tester;
  base::Value kOnlineStateValue(shill::kStateOnline);
  base::Value kIdleStateValue(shill::kStateIdle);

  // Should log connected state.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kPSimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kConnected, 1);
  histogram_tester.ExpectBucketCount(
      kESimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kConnected, 1);

  // Should not log user initiated disconnections.
  cellular_metrics_logger()->DisconnectRequested(kTestCellularServicePath);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kDisconnectRequestTimeout / 2);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kPSimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 0);
  histogram_tester.ExpectBucketCount(
      kESimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 0);

  // Should log non user initiated disconnects.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kStateProperty, kIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kPSimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 1);
  histogram_tester.ExpectBucketCount(
      kESimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 1);

  // Should log non user initiated disconnects when a previous
  // disconnect request timed out.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  cellular_metrics_logger()->DisconnectRequested(kTestCellularServicePath);
  cellular_metrics_logger()->DisconnectRequested(kTestCellularServicePath2);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kDisconnectRequestTimeout * 2);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kIdleStateValue);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kStateProperty, kIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kPSimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 2);
  histogram_tester.ExpectBucketCount(
      kESimDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 2);
}

}  // namespace chromeos
