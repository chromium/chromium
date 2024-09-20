// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/esim_policy_login_metrics_logger.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler_impl.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kTestEuiccPath[] = "euicc_path";
const char kTestEidName[] = "eid";
const char kTestESimGuid[] = "esim_guid";
const char kTestESimPolicyGuid[] = "esim_policy_guid";

const char kTestCellularDevicePath[] = "/device/wwan0";
const char kTestESimCellularServicePath[] = "/service/cellular1";
const char kTestESimPolicyCellularServicePath[] = "/service/cellular2";

}  // namespace

class ESimPolicyLoginMetricsLoggerTest : public testing::Test {
 public:
  ESimPolicyLoginMetricsLoggerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ESimPolicyLoginMetricsLoggerTest(const ESimPolicyLoginMetricsLoggerTest&) =
      delete;
  ESimPolicyLoginMetricsLoggerTest& operator=(
      const ESimPolicyLoginMetricsLoggerTest&) = delete;

  ~ESimPolicyLoginMetricsLoggerTest() override = default;

  void SetUp() override {
    LoginState::Initialize();

    network_state_test_helper_.hermes_manager_test()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEidName, /*is_active=*/true,
        /*physical_slot=*/0);

    network_config_handler_ = NetworkConfigurationHandler::InitializeForTest(
        network_state_test_helper_.network_state_handler(),
        /*network_device_handler=*/nullptr);

    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();

    managed_config_handler_.reset(new ManagedNetworkConfigurationHandlerImpl());
    managed_config_handler_->Init(
        /*cellular_policy_handler=*/nullptr,
        /*managed_cellular_pref_handler=*/nullptr,
        network_state_test_helper_.network_state_handler(),
        network_profile_handler_.get(), network_config_handler_.get(),
        /*network_device_handler=*/nullptr,
        /*prohibited_technologies_handler=*/nullptr,
        /*hotspot_controller=*/nullptr);

    esim_policy_login_metrics_logger_ =
        std::make_unique<ESimPolicyLoginMetricsLogger>();
    esim_policy_login_metrics_logger_->Init(
        network_state_test_helper_.network_state_handler(),
        managed_config_handler_.get());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_state_test_helper_.ClearDevices();
    network_state_test_helper_.ClearServices();
    managed_config_handler_.reset();
    network_profile_handler_.reset();
    network_config_handler_.reset();
    esim_policy_login_metrics_logger_.reset();
    LoginState::Shutdown();
  }

 protected:
  void ResetLogin() {
    LoginState::Get()->SetLoggedInState(
        LoginState::LoggedInState::LOGGED_IN_NONE,
        LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);
    LoginState::Get()->SetLoggedInState(
        LoginState::LoggedInState::LOGGED_IN_ACTIVE,
        LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);
  }

  void RemoveCellular() {
    ShillDeviceClient::TestInterface* device_test =
        network_state_test_helper_.device_test();
    device_test->RemoveDevice(kTestCellularDevicePath);
    base::RunLoop().RunUntilIdle();
  }

  void SetGlobalPolicy(bool allow_only_policy_cellular) {
    auto global_config = base::Value::Dict().Set(
        ::onc::global_network_config::kAllowOnlyPolicyCellularNetworks,
        allow_only_policy_cellular);
    managed_config_handler_->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
        /*network_configs_onc=*/base::Value::List(), global_config);
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  NetworkStateTestHelper network_state_test_helper_{
      /*use_default_devices_and_services=*/false};
  std::unique_ptr<NetworkConfigurationHandler> network_config_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_config_handler_;
  std::unique_ptr<ESimPolicyLoginMetricsLogger>
      esim_policy_login_metrics_logger_;
};

TEST_F(ESimPolicyLoginMetricsLoggerTest, LoginMetricsTest) {
  scoped_feature_list_.InitAndDisableFeature(
      ash::features::kAllowApnModificationPolicy);
  // Perform this test as though this "device" is enterprise managed.
  esim_policy_login_metrics_logger_->SetIsEnterpriseManaged(
      /*is_enterprise_managed=*/true);
  LoginState::Get()->SetLoggedInState(
      LoginState::LoggedInState::LOGGED_IN_ACTIVE,
      LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR);
  histogram_tester_.ExpectTotalCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyBlockNonManagedCellularHistogram,
      0);
  histogram_tester_.ExpectTotalCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyStatusAtLoginHistogram, 0);

  // Should wait until initialization timeout before logging status.
  ShillDeviceClient::TestInterface* device_test =
      network_state_test_helper_.device_test();
  device_test->AddDevice(kTestCellularDevicePath, shill::kTypeCellular,
                         "test_cellular_device");

  ShillServiceClient::TestInterface* service_test =
      network_state_test_helper_.service_test();
  service_test->AddService(kTestESimCellularServicePath, kTestESimGuid,
                           "test_cellular_unmanaged", shill::kTypeCellular,
                           shill::kStateIdle, /*visible=*/true);
  base::RunLoop().RunUntilIdle();
  service_test->SetServiceProperty(
      kTestESimCellularServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  service_test->SetServiceProperty(kTestESimCellularServicePath,
                                   shill::kEidProperty,
                                   base::Value("test_eid"));
  base::RunLoop().RunUntilIdle();
  histogram_tester_.ExpectTotalCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyBlockNonManagedCellularHistogram,
      0);
  histogram_tester_.ExpectTotalCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyStatusAtLoginHistogram, 0);

  task_environment_.FastForwardBy(
      ESimPolicyLoginMetricsLogger::kInitializationTimeout);

  histogram_tester_.ExpectTotalCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyBlockNonManagedCellularHistogram,
      1);
  histogram_tester_.ExpectTotalCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyStatusAtLoginHistogram, 1);
  histogram_tester_.ExpectBucketCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyStatusAtLoginHistogram,
      ESimPolicyLoginMetricsLogger::ESimPolicyStatusAtLogin::kUnmanagedOnly, 1);
  histogram_tester_.ExpectBucketCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyBlockNonManagedCellularHistogram,
      ESimPolicyLoginMetricsLogger::BlockNonManagedCellularBehavior::
          kAllowUnmanaged,
      1);

  // Adding another managed network and set allow_only_policy_cellular_networks
  // to true
  SetGlobalPolicy(/*allow_only_policy_cellular=*/true);
  service_test->AddService(kTestESimPolicyCellularServicePath,
                           kTestESimPolicyGuid, "test_cellular_managed",
                           shill::kTypeCellular, shill::kStateIdle,
                           /*visible=*/true);
  service_test->SetServiceProperty(
      kTestESimPolicyCellularServicePath, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  service_test->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                   shill::kONCSourceProperty,
                                   base::Value(shill::kONCSourceDevicePolicy));
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
  service_test->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                   shill::kUIDataProperty,
                                   base::Value(ui_data->GetAsJson()));
  service_test->SetServiceProperty(kTestESimPolicyCellularServicePath,
                                   shill::kEidProperty,
                                   base::Value("test_eid"));
  base::RunLoop().RunUntilIdle();
  ResetLogin();
  task_environment_.FastForwardBy(
      ESimPolicyLoginMetricsLogger::kInitializationTimeout);
  histogram_tester_.ExpectTotalCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyBlockNonManagedCellularHistogram,
      3);
  histogram_tester_.ExpectTotalCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyStatusAtLoginHistogram, 2);
  histogram_tester_.ExpectBucketCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyStatusAtLoginHistogram,
      ESimPolicyLoginMetricsLogger::ESimPolicyStatusAtLogin::
          kManagedAndUnmanaged,
      1);
  histogram_tester_.ExpectBucketCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyBlockNonManagedCellularHistogram,
      ESimPolicyLoginMetricsLogger::BlockNonManagedCellularBehavior::
          kAllowUnmanaged,
      1);
  histogram_tester_.ExpectBucketCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyBlockNonManagedCellularHistogram,
      ESimPolicyLoginMetricsLogger::BlockNonManagedCellularBehavior::
          kAllowManagedOnly,
      2);

  // Verify that no metrics should be logged if the device is not enterprise
  // enrolled.
  esim_policy_login_metrics_logger_->SetIsEnterpriseManaged(
      /*is_enterprise_managed=*/false);
  ResetLogin();
  task_environment_.FastForwardBy(
      ESimPolicyLoginMetricsLogger::kInitializationTimeout);
  histogram_tester_.ExpectTotalCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyBlockNonManagedCellularHistogram,
      3);
  histogram_tester_.ExpectTotalCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyStatusAtLoginHistogram, 2);

  // Verify that no metrics should be logged if no cellular device is found
  RemoveCellular();
  esim_policy_login_metrics_logger_->SetIsEnterpriseManaged(
      /*is_enterprise_managed=*/true);
  ResetLogin();
  task_environment_.FastForwardBy(
      ESimPolicyLoginMetricsLogger::kInitializationTimeout);
  histogram_tester_.ExpectTotalCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyBlockNonManagedCellularHistogram,
      3);
  histogram_tester_.ExpectTotalCount(
      ESimPolicyLoginMetricsLogger::kESimPolicyStatusAtLoginHistogram, 2);
}

}  // namespace ash
