// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_esim_installer.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/cellular_connection_handler.h"
#include "chromeos/ash/components/network/cellular_inhibitor.h"
#include "chromeos/ash/components/network/fake_network_connection_handler.h"
#include "chromeos/ash/components/network/fake_stub_cellular_networks_provider.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_test_helper.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using InstallResultTuple = std::tuple<ash::HermesResponseStatus,
                                      std::optional<dbus::ObjectPath>,
                                      std::optional<std::string>>;

using ash::cellular_setup::mojom::ProfileInstallMethod;

namespace ash {
namespace {

const char kTestEuiccPath[] = "/org/chromium/Hermes/Euicc/0";
const char kTestEid[] = "12345678901234567890123456789012";
const char kTestCellularServicePath[] = "/service/cellular101";
const char kInstallViaQrCodeHistogram[] =
    "Network.Cellular.ESim.InstallViaQrCode.Result";
const char kInstallViaQrCodeDBusResultHistogram[] =
    "Network.Cellular.ESim.InstallViaQrCode.DBusResult";
const char kESimInstallNonUserErrorSuccessRate[] =
    "Network.Cellular.ESim.Installation.NonUserErrorSuccessRate";

const char kESimProfileDownloadLatencyHistogram[] =
    "Network.Cellular.ESim.ProfileDownload.ActivationCode.Latency";

base::Value::Dict GetPolicyShillProperties() {
  base::Value::Dict new_shill_properties;
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
  new_shill_properties.Set(shill::kUIDataProperty, ui_data->GetAsJson());
  return new_shill_properties;
}

}  // namespace

class CellularESimInstallerTest : public testing::Test {
 protected:
  ~CellularESimInstallerTest() override = default;

  // testing::Test:
  void SetUp() override {
    shill_clients::InitializeFakes();
    hermes_clients::InitializeFakes();

    network_state_handler_ = NetworkStateHandler::InitializeForTest();
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        network_state_handler_.get());
    network_connection_handler_ =
        std::make_unique<FakeNetworkConnectionHandler>();
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
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
    stub_cellular_networks_provider_ =
        std::make_unique<FakeStubCellularNetworksProvider>();
    network_state_handler_->set_stub_cellular_networks_provider(
        stub_cellular_networks_provider_.get());
    SetupEuicc();
  }

  void SetupEuicc() {
    HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEid, /*is_active=*/true,
        /*physical_slot=*/0);

    base::RunLoop().RunUntilIdle();
  }

  // testing::Test:
  void TearDown() override {
    stub_cellular_networks_provider_.reset();
    cellular_esim_installer_.reset();
    cellular_esim_profile_handler_.reset();
    cellular_connection_handler_.reset();
    cellular_inhibitor_.reset();
    network_profile_handler_.reset();
    network_device_handler_.reset();
    network_state_handler_.reset();
    network_connection_handler_.reset();

    hermes_clients::Shutdown();
    shill_clients::Shutdown();
  }

  InstallResultTuple InstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      const dbus::ObjectPath euicc_path,
      base::Value::Dict new_shill_properties,
      bool wait_for_connect,
      bool fail_connect,
      bool is_initial_install = true,
      ProfileInstallMethod install_method =
          ProfileInstallMethod::kViaActivationCodeAfterSmds,
      bool auto_connected = false) {
    HermesResponseStatus out_install_result;
    std::optional<dbus::ObjectPath> out_esim_profile_path;
    std::optional<std::string> out_service_path;

    base::RunLoop run_loop;
    cellular_esim_installer_->InstallProfileFromActivationCode(
        activation_code, confirmation_code, euicc_path,
        std::move(new_shill_properties),
        base::BindLambdaForTesting(
            [&](HermesResponseStatus install_result,
                std::optional<dbus::ObjectPath> esim_profile_path,
                std::optional<std::string> service_path) {
              out_install_result = install_result;
              out_esim_profile_path = esim_profile_path;
              out_service_path = service_path;
              run_loop.Quit();
            }),
        is_initial_install, install_method);

    FastForwardProfileRefreshDelay();

    if (wait_for_connect) {
      if (auto_connected) {
        ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
            kTestCellularServicePath, shill::kStateProperty,
            base::Value(shill::kStateOnline));
      } else {
        FastForwardAutoConnectWaiting();
        base::RunLoop().RunUntilIdle();
        EXPECT_LE(1u, network_connection_handler_->connect_calls().size());
        if (fail_connect) {
          network_connection_handler_->connect_calls()
              .back()
              .InvokeErrorCallback("fake_error_name");
        } else {
          network_connection_handler_->connect_calls()
              .back()
              .InvokeSuccessCallback();
        }
      }
    }

    run_loop.Run();
    return std::make_tuple(out_install_result, out_esim_profile_path,
                           out_service_path);
  }

  std::optional<dbus::ObjectPath> ConfigureESimService(
      const dbus::ObjectPath euicc_path,
      const dbus::ObjectPath& profile_path,
      base::Value::Dict& new_shill_properties) {
    std::optional<dbus::ObjectPath> service_path_out;
    base::RunLoop run_loop;
    cellular_esim_installer_->ConfigureESimService(
        new_shill_properties, euicc_path, profile_path,
        base::BindLambdaForTesting(
            [&](std::optional<dbus::ObjectPath> service_path) {
              service_path_out = service_path;
              run_loop.Quit();
            }));
    run_loop.Run();
    return service_path_out;
  }

  void CheckInstallSuccess(const InstallResultTuple& actual_result_tuple) {
    EXPECT_EQ(HermesResponseStatus::kSuccess, std::get<0>(actual_result_tuple));
    EXPECT_NE(std::get<1>(actual_result_tuple), std::nullopt);
    EXPECT_NE(std::get<2>(actual_result_tuple), std::nullopt);
    const base::Value::Dict* properties =
        ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
            *std::get<2>(actual_result_tuple));
    ASSERT_TRUE(properties);
    const std::string* type = properties->FindString(shill::kTypeProperty);
    EXPECT_EQ(shill::kTypeCellular, *type);
    const std::string* iccid = properties->FindString(shill::kIccidProperty);
    EXPECT_NE(std::string(), *iccid);
  }

  void CheckESimInstallHistograms(
      int expected_count,
      HermesResponseStatus expected_hermes_status,
      CellularESimInstaller::InstallESimProfileResult expected_install_result) {
    histogram_tester()->ExpectBucketCount(
        kInstallViaQrCodeHistogram, expected_hermes_status, expected_count);

    if (expected_hermes_status == HermesResponseStatus::kSuccess ||
        !base::Contains(kHermesUserErrorCodes, expected_hermes_status)) {
      histogram_tester()->ExpectBucketCount(kESimInstallNonUserErrorSuccessRate,
                                            expected_hermes_status,
                                            expected_count);
    } else {
      histogram_tester()->ExpectBucketCount(kESimInstallNonUserErrorSuccessRate,
                                            expected_hermes_status, 0);
    }
    if (expected_hermes_status != HermesResponseStatus::kErrorUnknownResponse) {
      histogram_tester()->ExpectTotalCount(kInstallViaQrCodeDBusResultHistogram,
                                           0);
    }
  }


  void FastForwardProfileRefreshDelay() {
    const base::TimeDelta kProfileRefreshCallbackDelay =
        base::Milliseconds(150);

    // Connect can result in two profile refresh calls before and after
    // enabling profile. Fast forward by delay after refresh.
    task_environment_.FastForwardBy(2 * kProfileRefreshCallbackDelay);
  }

  void FastForwardAutoConnectWaiting() {
    task_environment_.FastForwardBy(
        CellularConnectionHandler::kWaitingForAutoConnectTimeout);
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<CellularConnectionHandler> cellular_connection_handler_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
  std::unique_ptr<FakeNetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<CellularESimInstaller> cellular_esim_installer_;
  std::unique_ptr<FakeStubCellularNetworksProvider>
      stub_cellular_networks_provider_;
};

TEST_F(CellularESimInstallerTest, InstallProfileInvalidActivationCode) {
  ash::cellular_metrics::ESimInstallHistogramState state;
  state.Check(histogram_tester());

  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      /*activation_code=*/std::string(), /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value::Dict(),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(HermesResponseStatus::kErrorInvalidActivationCode,
            std::get<0>(result_tuple));
  EXPECT_EQ(std::get<1>(result_tuple), std::nullopt);
  EXPECT_EQ(std::get<2>(result_tuple), std::nullopt);
  CheckESimInstallHistograms(
      /*expected_count=*/1, HermesResponseStatus::kErrorInvalidActivationCode,
      CellularESimInstaller::InstallESimProfileResult::kHermesInstallFailed);

  state.user_install_user_errors_included_all.hermes_failed_count++;
  state.user_install_user_errors_included_via_activation_code_after_smds
      .hermes_failed_count++;
  state.Check(histogram_tester());

  // Verify that install from policy are handled properly.
  result_tuple = InstallProfileFromActivationCode(
      /*activation_code=*/std::string(), /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      GetPolicyShillProperties(),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(HermesResponseStatus::kErrorInvalidActivationCode,
            std::get<0>(result_tuple));
  EXPECT_EQ(std::get<1>(result_tuple), std::nullopt);
  EXPECT_EQ(std::get<2>(result_tuple), std::nullopt);
  CheckESimInstallHistograms(
      /*expected_count=*/2, HermesResponseStatus::kErrorInvalidActivationCode,
      CellularESimInstaller::InstallESimProfileResult::kHermesInstallFailed);

  state.policy_install_user_errors_included_all.hermes_failed_count++;
  state.policy_install_user_errors_included_smdp_initial.hermes_failed_count++;
  state.Check(histogram_tester());
}

TEST_F(CellularESimInstallerTest, InstallProfileDBusError) {
  ash::cellular_metrics::ESimInstallHistogramState state;
  state.Check(histogram_tester());

  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GetDBusErrorActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value::Dict(),
      /*wait_for_connect=*/false, /*fail_connect=*/false);

  histogram_tester()->ExpectTotalCount(kInstallViaQrCodeDBusResultHistogram, 1);
  histogram_tester()->ExpectBucketCount(kInstallViaQrCodeDBusResultHistogram,
                                        dbus::DBusResult::kErrorNoMemory, 1);
  histogram_tester()->ExpectBucketCount(
      kInstallViaQrCodeHistogram, HermesResponseStatus::kErrorUnknownResponse,
      1);

  state.user_install_user_errors_filtered_all.hermes_failed_count++;
  state.user_install_user_errors_filtered_via_activation_code_after_smds
      .hermes_failed_count++;
  state.user_install_user_errors_included_all.hermes_failed_count++;
  state.user_install_user_errors_included_via_activation_code_after_smds
      .hermes_failed_count++;
  state.Check(histogram_tester());
}

TEST_F(CellularESimInstallerTest, InstallProfileConnectFailure) {
  ash::cellular_metrics::ESimInstallHistogramState state;
  state.Check(histogram_tester());

  // Verify that connect failures are handled properly.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value::Dict(),
      /*wait_for_connect=*/true, /*fail_connect=*/true);
  CheckInstallSuccess(result_tuple);
  CheckESimInstallHistograms(
      /*expected_count=*/1, HermesResponseStatus::kSuccess,
      CellularESimInstaller::InstallESimProfileResult::kSuccess);

  state.user_install_user_errors_filtered_all.success_count++;
  state.user_install_user_errors_filtered_via_activation_code_after_smds
      .success_count++;
  state.user_install_user_errors_included_all.success_count++;
  state.user_install_user_errors_included_via_activation_code_after_smds
      .success_count++;
  state.Check(histogram_tester());

  // Verify that install from policy are handled property.
  result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      GetPolicyShillProperties(),
      /*wait_for_connect=*/true, /*fail_connect=*/true);
  CheckInstallSuccess(result_tuple);
  CheckESimInstallHistograms(
      /*expected_count=*/2, HermesResponseStatus::kSuccess,
      CellularESimInstaller::InstallESimProfileResult::kSuccess);

  state.policy_install_user_errors_filtered_all.success_count++;
  state.policy_install_user_errors_filtered_smdp_initial.success_count++;
  state.policy_install_user_errors_included_all.success_count++;
  state.policy_install_user_errors_included_smdp_initial.success_count++;
  state.Check(histogram_tester());
}

TEST_F(CellularESimInstallerTest, InstallProfileSuccess) {
  ash::cellular_metrics::ESimInstallHistogramState state;
  state.Check(histogram_tester());

  // Verify that install succeeds when valid activation code is passed.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value::Dict(),
      /*wait_for_connect=*/true, /*fail_connect=*/false);
  CheckInstallSuccess(result_tuple);

  histogram_tester()->ExpectTotalCount(kESimProfileDownloadLatencyHistogram, 1);
  CheckESimInstallHistograms(
      /*expected_count=*/1, HermesResponseStatus::kSuccess,
      CellularESimInstaller::InstallESimProfileResult::kSuccess);

  state.user_install_user_errors_filtered_all.success_count++;
  state.user_install_user_errors_filtered_via_activation_code_after_smds
      .success_count++;
  state.user_install_user_errors_included_all.success_count++;
  state.user_install_user_errors_included_via_activation_code_after_smds
      .success_count++;
  state.Check(histogram_tester());

  // Verify install from policy works properly.
  result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      GetPolicyShillProperties(),
      /*wait_for_connect=*/true, /*fail_connect=*/false,
      /*is_initial_install=*/false,
      /*install_method=*/ProfileInstallMethod::kViaSmds);
  CheckInstallSuccess(result_tuple);
  histogram_tester()->ExpectTotalCount(kESimProfileDownloadLatencyHistogram, 2);
  CheckESimInstallHistograms(
      /*expected_count=*/2, HermesResponseStatus::kSuccess,
      CellularESimInstaller::InstallESimProfileResult::kSuccess);

  state.policy_install_user_errors_filtered_all.success_count++;
  state.policy_install_user_errors_filtered_smds_retry.success_count++;
  state.policy_install_user_errors_included_all.success_count++;
  state.policy_install_user_errors_included_smds_retry.success_count++;
  state.Check(histogram_tester());
}

TEST_F(CellularESimInstallerTest, InstallProfileViaQrCodeSuccess) {
  ash::cellular_metrics::ESimInstallHistogramState state;
  state.Check(histogram_tester());

  // Verify that install succeeds when valid activation code is passed.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value::Dict(),
      /*wait_for_connect=*/true, /*fail_connect=*/false,
      /*is_initial_install=*/true,
      /*install_method=*/ProfileInstallMethod::kViaQrCodeAfterSmds);
  CheckInstallSuccess(result_tuple);

  histogram_tester()->ExpectTotalCount(kESimProfileDownloadLatencyHistogram, 1);
  CheckESimInstallHistograms(
      /*expected_count=*/1, HermesResponseStatus::kSuccess,
      CellularESimInstaller::InstallESimProfileResult::kSuccess);

  state.user_install_user_errors_filtered_all.success_count++;
  state.user_install_user_errors_filtered_via_qr_code_after_smds
      .success_count++;
  state.user_install_user_errors_included_all.success_count++;
  state.user_install_user_errors_included_via_qr_code_after_smds
      .success_count++;
  state.Check(histogram_tester());
}

TEST_F(CellularESimInstallerTest, InstallProfileAutoConnect) {
  ash::cellular_metrics::ESimInstallHistogramState state;
  state.Check(histogram_tester());

  // Verify that install succeeds when valid activation code is passed.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value::Dict(),
      /*wait_for_connect=*/true, /*fail_connect=*/false,
      /*is_initial_install=*/true,
      /*install_method=*/ProfileInstallMethod::kViaQrCodeSkippedSmds,
      /*auto_connected=*/true);
  CheckInstallSuccess(result_tuple);

  histogram_tester()->ExpectTotalCount(kESimProfileDownloadLatencyHistogram, 1);
  CheckESimInstallHistograms(
      /*expected_count=*/1, HermesResponseStatus::kSuccess,
      CellularESimInstaller::InstallESimProfileResult::kSuccess);

  state.user_install_user_errors_filtered_all.success_count++;
  state.user_install_user_errors_filtered_via_qr_code_skipped_smds
      .success_count++;
  state.user_install_user_errors_included_all.success_count++;
  state.user_install_user_errors_included_via_qr_code_skipped_smds
      .success_count++;
  state.Check(histogram_tester());
}

TEST_F(CellularESimInstallerTest, InstallProfileAlreadyConnected) {
  ash::cellular_metrics::ESimInstallHistogramState state;
  state.Check(histogram_tester());

  HermesProfileClient::Get()->GetTestInterface()->SetEnableProfileBehavior(
      HermesProfileClient::TestInterface::EnableProfileBehavior::
          kConnectableAndConnected);

  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value::Dict(),
      /*wait_for_connect=*/false, /*fail_connect=*/false,
      /*is_initial_install=*/true,
      /*install_method=*/ProfileInstallMethod::kViaActivationCodeSkippedSmds);
  CheckInstallSuccess(result_tuple);

  state.user_install_user_errors_filtered_all.success_count++;
  state.user_install_user_errors_filtered_via_activation_code_skipped_smds
      .success_count++;
  state.user_install_user_errors_included_all.success_count++;
  state.user_install_user_errors_included_via_activation_code_skipped_smds
      .success_count++;
  state.Check(histogram_tester());
}

TEST_F(CellularESimInstallerTest, InstallProfileCreateShillConfigFailure) {
  ash::cellular_metrics::ESimInstallHistogramState state;
  state.Check(histogram_tester());

  ShillManagerClient::Get()->GetTestInterface()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kFailure);
  // Verify that install succeeds when valid activation code is passed.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value::Dict(),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  CheckInstallSuccess(result_tuple);

  state.user_install_user_errors_filtered_all.success_count++;
  state.user_install_user_errors_filtered_via_activation_code_after_smds
      .success_count++;
  state.user_install_user_errors_included_all.success_count++;
  state.user_install_user_errors_included_via_activation_code_after_smds
      .success_count++;
  state.Check(histogram_tester());
}

TEST_F(CellularESimInstallerTest, ConfigureESimService) {
  dbus::ObjectPath profile_path =
      HermesEuiccClient::Get()->GetTestInterface()->AddFakeCarrierProfile(
          dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kInactive,
          /*activation_code=*/"",
          HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
              kAddProfileWithoutService);

  base::Value::Dict new_shill_properties;
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
  new_shill_properties.Set(shill::kUIDataProperty, ui_data->GetAsJson());
  std::optional<dbus::ObjectPath> service_path = ConfigureESimService(
      dbus::ObjectPath(kTestEuiccPath), profile_path, new_shill_properties);
  EXPECT_TRUE(service_path.has_value());

  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);
  const base::Value::Dict* service_properties =
      ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
          service_path->value());
  ASSERT_TRUE(service_properties);
  const std::string* type =
      service_properties->FindString(shill::kTypeProperty);
  EXPECT_EQ(shill::kTypeCellular, *type);
  const std::string* iccid =
      service_properties->FindString(shill::kIccidProperty);
  EXPECT_EQ(profile_properties->iccid().value(), *iccid);
  const std::string* eid = service_properties->FindString(shill::kEidProperty);
  EXPECT_EQ(kTestEid, *eid);
}

TEST_F(CellularESimInstallerTest, ConfigureESimServiceFailure) {
  dbus::ObjectPath profile_path =
      HermesEuiccClient::Get()->GetTestInterface()->AddFakeCarrierProfile(
          dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kInactive,
          /*activation_code=*/"",
          HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
              kAddProfileWithoutService);

  ShillManagerClient::Get()->GetTestInterface()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kFailure);

  base::Value::Dict new_shill_properties;
  std::optional<dbus::ObjectPath> service_path = ConfigureESimService(
      dbus::ObjectPath(kTestEuiccPath), profile_path, new_shill_properties);
  EXPECT_FALSE(service_path.has_value());
}

}  // namespace ash
