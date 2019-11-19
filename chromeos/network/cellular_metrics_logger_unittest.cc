// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_metrics_logger.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kTestCellularDevicePath[] = "/device/wwan0";
const char kTestCellularServicePath[] = "/service/cellular0";
const char kTestCellularServicePath2[] = "/service/cellular1";
const char kTestEthServicePath[] = "/service/eth0";

const char kUsageCountHistogram[] = "Network.Cellular.Usage.Count";
const char kActivationStatusAtLoginHistogram[] =
    "Network.Cellular.Activation.StatusAtLogin";
const char kTimeToConnectedHistogram[] =
    "Network.Cellular.Connection.TimeToConnected";
const char kDisconnectionsHistogram[] =
    "Network.Cellular.Connection.Disconnections";

}  // namespace

class CellularMetricsLoggerTest : public testing::Test {
 public:
  CellularMetricsLoggerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~CellularMetricsLoggerTest() override = default;

  void SetUp() override {
    LoginState::Initialize();

    cellular_metrics_logger_.reset(new CellularMetricsLogger());
    cellular_metrics_logger_->Init(
        network_state_test_helper_.network_state_handler(),
        /* network_connection_handler */ nullptr);
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

 private:
  NetworkStateTestHelper network_state_test_helper_{
      false /* use_default_devices_and_services */};
  std::unique_ptr<CellularMetricsLogger> cellular_metrics_logger_;

  DISALLOW_COPY_AND_ASSIGN(CellularMetricsLoggerTest);
};

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
  histogram_tester.ExpectTotalCount(kUsageCountHistogram, 0);

  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram, CellularMetricsLogger::CellularUsage::kNotConnected,
      1);

  // Cellular connected with other network.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);

  // Cellular connected as only network.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);

  // Cellular not connected.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram, CellularMetricsLogger::CellularUsage::kNotConnected,
      2);
}

TEST_F(CellularMetricsLoggerTest, CellularUsageCountDongleTest) {
  InitEthernet();
  base::HistogramTester histogram_tester;
  static const base::Value kTestOnlineStateValue(shill::kStateOnline);
  static const base::Value kTestIdleStateValue(shill::kStateIdle);

  // Should not log state if no cellular devices are available.
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectTotalCount(kUsageCountHistogram, 0);

  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kUsageCountHistogram, 0);

  // Should log state if a new cellular device is plugged in.
  InitCellular();
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram, CellularMetricsLogger::CellularUsage::kNotConnected,
      1);

  // Should log connected state for cellular device that was plugged in.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kTestOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedWithOtherNetwork, 1);

  // Should log connected as only network state for cellular device
  // that was plugged in.
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUsageCountHistogram,
      CellularMetricsLogger::CellularUsage::kConnectedAndOnlyNetwork, 1);

  // Should not log state if cellular device is unplugged.
  RemoveCellular();
  service_client_test()->SetServiceProperty(
      kTestEthServicePath, shill::kStateProperty, kTestIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kUsageCountHistogram, 3);
}

TEST_F(CellularMetricsLoggerTest, CellularActivationStateAtLoginTest) {
  base::HistogramTester histogram_tester;

  // Should defer logging when there are no cellular networks.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  histogram_tester.ExpectTotalCount(kActivationStatusAtLoginHistogram, 0);

  // Should wait until initialization timeout before logging status.
  InitCellular();
  histogram_tester.ExpectTotalCount(kActivationStatusAtLoginHistogram, 0);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kInitializationTimeout);
  histogram_tester.ExpectBucketCount(
      kActivationStatusAtLoginHistogram,
      CellularMetricsLogger::ActivationState::kNotActivated, 1);

  // Should log immediately when networks are already initialized.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_NONE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  histogram_tester.ExpectBucketCount(
      kActivationStatusAtLoginHistogram,
      CellularMetricsLogger::ActivationState::kNotActivated, 2);

  // Should log activated state.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  base::RunLoop().RunUntilIdle();
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_NONE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER);
  histogram_tester.ExpectBucketCount(
      kActivationStatusAtLoginHistogram,
      CellularMetricsLogger::ActivationState::kActivated, 1);

  // Should not log when the logged in user is neither owner nor regular.
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_KIOSK_APP);
  histogram_tester.ExpectTotalCount(kActivationStatusAtLoginHistogram, 3);
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
  histogram_tester.ExpectTotalCount(kTimeToConnectedHistogram, 0);

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
  histogram_tester.ExpectTimeBucketCount(kTimeToConnectedHistogram,
                                         kTestConnectionTime, 1);

  // Should log second network's connection time independently.
  task_environment_.FastForwardBy(kTestConnectionTime);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath2, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTimeBucketCount(kTimeToConnectedHistogram,
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
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kConnected, 1);

  // Should not log user initiated disconnections.
  cellular_metrics_logger()->DisconnectRequested(kTestCellularServicePath);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kDisconnectRequestTimeout / 2);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 0);

  // Should log non user initiated disconnects.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 1);

  // Should log non user initiated disconnects when a previous
  // disconnect request timed out.
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kOnlineStateValue);
  base::RunLoop().RunUntilIdle();
  cellular_metrics_logger()->DisconnectRequested(kTestCellularServicePath);
  task_environment_.FastForwardBy(
      CellularMetricsLogger::kDisconnectRequestTimeout * 2);
  service_client_test()->SetServiceProperty(
      kTestCellularServicePath, shill::kStateProperty, kIdleStateValue);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kDisconnectionsHistogram,
      CellularMetricsLogger::ConnectionState::kDisconnected, 2);
}

}  // namespace chromeos
