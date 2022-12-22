// Copyright 2021 The Chromium Authors
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
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

using InstallResultTuple = std::tuple<HermesResponseStatus,
                                      absl::optional<dbus::ObjectPath>,
                                      absl::optional<std::string>>;

const char kTestEuiccPath[] = "/org/chromium/Hermes/Euicc/0";
const char kTestEid[] = "12345678901234567890123456789012";
const char kTestCellularServicePath[] = "/service/cellular101";
const char kInstallViaQrCodeHistogram[] =
    "Network.Cellular.ESim.InstallViaQrCode.Result";
const char kESimInstallNonUserErrorSuccessRate[] =
    "Network.Cellular.ESim.Installation.NonUserErrorSuccessRate";

const char kUserInstallOperationHistogram[] =
    "Network.Cellular.ESim.UserInstall.OperationResult.All";
const char kUserInstallViaQrCodeOperationHistogram[] =
    "Network.Cellular.ESim.UserInstall.OperationResult.ViaQrCode";
const char kUserInstallViaCodeInputOperationHistogram[] =
    "Network.Cellular.ESim.UserInstall.OperationResult.ViaCodeInput";

const char kInstallViaPolicyOperationHistogram[] =
    "Network.Cellular.ESim.Policy.ESimInstall.OperationResult";
const char kInstallViaPolicyInitialOperationHistogram[] =
    "Network.Cellular.ESim.Policy.ESimInstall.OperationResult.InitialAttempt";
const char kInstallViaPolicyRetryOperationHistogram[] =
    "Network.Cellular.ESim.Policy.ESimInstall.OperationResult.Retry";
const char kInstallESimResultHistogram[] =
    "Network.Cellular.ESim.InstallationResult";
const char kESimProfileDownloadLatencyHistogram[] =
    "Network.Cellular.ESim.ProfileDownload.ActivationCode.Latency";

base::Value GetPolicyShillProperties() {
  base::Value new_shill_properties(base::Value::Type::DICTIONARY);
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
  new_shill_properties.SetStringKey(shill::kUIDataProperty,
                                    ui_data->GetAsJson());
  return new_shill_properties;
}

}  // namespace

class CellularESimInstallerTest : public testing::Test {
 protected:
  CellularESimInstallerTest() = default;
  ~CellularESimInstallerTest() override = default;

  // testing::Test
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

  // testing::Test
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
      base::Value new_shill_properties,
      bool wait_for_connect,
      bool fail_connect,
      bool is_initial_install = true,
      bool is_install_via_qr_code = false,
      bool auto_connected = false) {
    HermesResponseStatus out_install_result;
    absl::optional<dbus::ObjectPath> out_esim_profile_path;
    absl::optional<std::string> out_service_path;

    base::RunLoop run_loop;
    cellular_esim_installer_->InstallProfileFromActivationCode(
        activation_code, confirmation_code, euicc_path,
        std::move(new_shill_properties),
        base::BindLambdaForTesting(
            [&](HermesResponseStatus install_result,
                absl::optional<dbus::ObjectPath> esim_profile_path,
                absl::optional<std::string> service_path) {
              out_install_result = install_result;
              out_esim_profile_path = esim_profile_path;
              out_service_path = service_path;
              run_loop.Quit();
            }),
        is_initial_install, is_install_via_qr_code);

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

  absl::optional<dbus::ObjectPath> ConfigureESimService(
      const dbus::ObjectPath euicc_path,
      const dbus::ObjectPath& profile_path,
      base::Value& new_shill_properties) {
    absl::optional<dbus::ObjectPath> service_path_out;
    base::RunLoop run_loop;
    cellular_esim_installer_->ConfigureESimService(
        new_shill_properties, euicc_path, profile_path,
        base::BindLambdaForTesting(
            [&](absl::optional<dbus::ObjectPath> service_path) {
              service_path_out = service_path;
              run_loop.Quit();
            }));
    run_loop.Run();
    return service_path_out;
  }

  void CheckInstallSuccess(const InstallResultTuple& actual_result_tuple) {
    EXPECT_EQ(HermesResponseStatus::kSuccess, std::get<0>(actual_result_tuple));
    EXPECT_NE(std::get<1>(actual_result_tuple), absl::nullopt);
    EXPECT_NE(std::get<2>(actual_result_tuple), absl::nullopt);
    const base::Value* properties =
        ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
            *std::get<2>(actual_result_tuple));
    ASSERT_TRUE(properties);
    const std::string* type = properties->FindStringKey(shill::kTypeProperty);
    EXPECT_EQ(shill::kTypeCellular, *type);
    const std::string* iccid = properties->FindStringKey(shill::kIccidProperty);
    EXPECT_NE(std::string(), *iccid);
  }

  void CheckESimInstallHistograms(
      int expected_count,
      HermesResponseStatus expected_hermes_status,
      CellularESimInstaller::InstallESimProfileResult expected_install_result) {
    HistogramTesterPtr()->ExpectBucketCount(
        kInstallViaQrCodeHistogram, expected_hermes_status, expected_count);
    HistogramTesterPtr()->ExpectBucketCount(
        kInstallESimResultHistogram, expected_install_result, expected_count);

    if (expected_hermes_status == HermesResponseStatus::kSuccess ||
        !base::Contains(kHermesUserErrorCodes, expected_hermes_status)) {
      HistogramTesterPtr()->ExpectBucketCount(
          kESimInstallNonUserErrorSuccessRate, expected_hermes_status,
          expected_count);
    } else {
      HistogramTesterPtr()->ExpectBucketCount(
          kESimInstallNonUserErrorSuccessRate, expected_hermes_status, 0);
    }
  }

  void CheckDetailedESimInstallHistograms(
      CellularESimInstaller::InstallESimProfileResult expected_result,
      bool is_managed = false,
      bool is_retry = false,
      bool is_install_via_qr_code = false) {
    HistogramTesterPtr()->ExpectBucketCount(kUserInstallOperationHistogram,
                                            expected_result,
                                            /*expected_count=*/1);
    HistogramTesterPtr()->ExpectBucketCount(
        is_install_via_qr_code ? kUserInstallViaQrCodeOperationHistogram
                               : kUserInstallViaCodeInputOperationHistogram,
        expected_result,
        /*expected_count=*/1);

    int expected_policy_histogram_counts = 0;
    int expected_policy_retry_counts = 0;
    if (is_managed) {
      expected_policy_histogram_counts = 1;
      if (is_retry) {
        expected_policy_retry_counts = 1;
      }
    }
    HistogramTesterPtr()->ExpectBucketCount(kInstallViaPolicyOperationHistogram,
                                            expected_result,
                                            expected_policy_histogram_counts);
    HistogramTesterPtr()->ExpectBucketCount(
        kInstallViaPolicyInitialOperationHistogram, expected_result,
        expected_policy_histogram_counts);
    HistogramTesterPtr()->ExpectBucketCount(
        kInstallViaPolicyRetryOperationHistogram, expected_result,
        expected_policy_retry_counts);
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

  base::HistogramTester* HistogramTesterPtr() { return &histogram_tester_; }

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
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      /*activation_code=*/std::string(), /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value(base::Value::Type::DICTIONARY),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(HermesResponseStatus::kErrorInvalidActivationCode,
            std::get<0>(result_tuple));
  EXPECT_EQ(std::get<1>(result_tuple), absl::nullopt);
  EXPECT_EQ(std::get<2>(result_tuple), absl::nullopt);
  CheckESimInstallHistograms(
      /*expected_count=*/1, HermesResponseStatus::kErrorInvalidActivationCode,
      CellularESimInstaller::InstallESimProfileResult::kHermesInstallFailed);
  CheckDetailedESimInstallHistograms(
      CellularESimInstaller::InstallESimProfileResult::kHermesInstallFailed);

  // Verify that install from policy are handled properly
  base::Value new_shill_properties(base::Value::Type::DICTIONARY);
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
  new_shill_properties.SetStringKey(shill::kUIDataProperty,
                                    ui_data->GetAsJson());
  result_tuple = InstallProfileFromActivationCode(
      /*activation_code=*/std::string(), /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      GetPolicyShillProperties(),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(HermesResponseStatus::kErrorInvalidActivationCode,
            std::get<0>(result_tuple));
  EXPECT_EQ(std::get<1>(result_tuple), absl::nullopt);
  EXPECT_EQ(std::get<2>(result_tuple), absl::nullopt);
  CheckESimInstallHistograms(
      /*expected_count=*/2, HermesResponseStatus::kErrorInvalidActivationCode,
      CellularESimInstaller::InstallESimProfileResult::kHermesInstallFailed);
  CheckDetailedESimInstallHistograms(
      CellularESimInstaller::InstallESimProfileResult::kHermesInstallFailed,
      /*is_managed=*/true);
  HistogramTesterPtr()->ExpectTotalCount(
      kInstallViaPolicyRetryOperationHistogram, 0);
}

TEST_F(CellularESimInstallerTest, InstallProfileConnectFailure) {
  // Verify that connect failures are handled properly.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value(base::Value::Type::DICTIONARY),
      /*wait_for_connect=*/true, /*fail_connect=*/true);
  CheckInstallSuccess(result_tuple);
  CheckESimInstallHistograms(
      /*expected_count=*/1, HermesResponseStatus::kSuccess,
      CellularESimInstaller::InstallESimProfileResult::kSuccess);
  CheckDetailedESimInstallHistograms(
      CellularESimInstaller::InstallESimProfileResult::kSuccess);

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
  CheckDetailedESimInstallHistograms(
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*is_managed=*/true);
}

TEST_F(CellularESimInstallerTest, InstallProfileSuccess) {
  // Verify that install succeeds when valid activation code is passed.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value(base::Value::Type::DICTIONARY),
      /*wait_for_connect=*/true, /*fail_connect=*/false);
  CheckInstallSuccess(result_tuple);

  HistogramTesterPtr()->ExpectTotalCount(kESimProfileDownloadLatencyHistogram,
                                         1);
  CheckESimInstallHistograms(
      /*expected_count=*/1, HermesResponseStatus::kSuccess,
      CellularESimInstaller::InstallESimProfileResult::kSuccess);
  CheckDetailedESimInstallHistograms(
      CellularESimInstaller::InstallESimProfileResult::kSuccess);

  // Verify install from policy works properly
  result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      GetPolicyShillProperties(),
      /*wait_for_connect=*/true, /*fail_connect=*/false);
  CheckInstallSuccess(result_tuple);
  HistogramTesterPtr()->ExpectTotalCount(kESimProfileDownloadLatencyHistogram,
                                         2);
  CheckESimInstallHistograms(
      /*expected_count=*/2, HermesResponseStatus::kSuccess,
      CellularESimInstaller::InstallESimProfileResult::kSuccess);
  CheckDetailedESimInstallHistograms(
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*is_managed=*/true);
}

TEST_F(CellularESimInstallerTest, InstallProfileViaQrCodeSuccess) {
  // Verify that install succeeds when valid activation code is passed.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value(base::Value::Type::DICTIONARY),
      /*wait_for_connect=*/true, /*fail_connect=*/false,
      /*is_initial_install=*/true, /*is_install_via_qr_code=*/true);
  CheckInstallSuccess(result_tuple);

  HistogramTesterPtr()->ExpectTotalCount(kESimProfileDownloadLatencyHistogram,
                                         1);
  CheckESimInstallHistograms(
      /*expected_count=*/1, HermesResponseStatus::kSuccess,
      CellularESimInstaller::InstallESimProfileResult::kSuccess);
  CheckDetailedESimInstallHistograms(
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*is_managed=*/false, /*is_retry=*/false,
      /*is_install_via_qr_code=*/true);
}

TEST_F(CellularESimInstallerTest, InstallProfileAutoConnect) {
  // Verify that install succeeds when valid activation code is passed.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value(base::Value::Type::DICTIONARY),
      /*wait_for_connect=*/true, /*fail_connect=*/false,
      /*is_initial_install=*/true, /*is_install_via_qr_code=*/true,
      /*auto_connected=*/true);
  CheckInstallSuccess(result_tuple);

  HistogramTesterPtr()->ExpectTotalCount(kESimProfileDownloadLatencyHistogram,
                                         1);
  CheckESimInstallHistograms(
      /*expected_count=*/1, HermesResponseStatus::kSuccess,
      CellularESimInstaller::InstallESimProfileResult::kSuccess);
  CheckDetailedESimInstallHistograms(
      CellularESimInstaller::InstallESimProfileResult::kSuccess,
      /*is_managed=*/false, /*is_retry=*/false,
      /*is_install_via_qr_code=*/true);
}

TEST_F(CellularESimInstallerTest, InstallProfileAlreadyConnected) {
  HermesProfileClient::Get()->GetTestInterface()->SetEnableProfileBehavior(
      HermesProfileClient::TestInterface::EnableProfileBehavior::
          kConnectableAndConnected);

  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value(base::Value::Type::DICTIONARY),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  CheckInstallSuccess(result_tuple);
}

TEST_F(CellularESimInstallerTest, InstallProfileCreateShillConfigFailure) {
  ShillManagerClient::Get()->GetTestInterface()->SetSimulateConfigurationResult(
      FakeShillSimulatedResult::kFailure);
  // Verify that install succeeds when valid activation code is passed.
  InstallResultTuple result_tuple = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*new_shill_properties=*/base::Value(base::Value::Type::DICTIONARY),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  CheckInstallSuccess(result_tuple);
}

TEST_F(CellularESimInstallerTest, ConfigureESimService) {
  dbus::ObjectPath profile_path =
      HermesEuiccClient::Get()->GetTestInterface()->AddFakeCarrierProfile(
          dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kInactive,
          /*activation_code=*/"",
          HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
              kAddProfileWithoutService);

  base::Value new_shill_properties(base::Value::Type::DICTIONARY);
  std::unique_ptr<NetworkUIData> ui_data =
      NetworkUIData::CreateFromONC(::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
  new_shill_properties.SetStringKey(shill::kUIDataProperty,
                                    ui_data->GetAsJson());
  absl::optional<dbus::ObjectPath> service_path = ConfigureESimService(
      dbus::ObjectPath(kTestEuiccPath), profile_path, new_shill_properties);
  EXPECT_TRUE(service_path.has_value());

  HermesProfileClient::Properties* profile_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);
  const base::Value* service_properties =
      ShillServiceClient::Get()->GetTestInterface()->GetServiceProperties(
          service_path->value());
  ASSERT_TRUE(service_properties);
  const std::string* type =
      service_properties->FindStringKey(shill::kTypeProperty);
  EXPECT_EQ(shill::kTypeCellular, *type);
  const std::string* iccid =
      service_properties->FindStringKey(shill::kIccidProperty);
  EXPECT_EQ(profile_properties->iccid().value(), *iccid);
  const std::string* eid =
      service_properties->FindStringKey(shill::kEidProperty);
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

  base::Value new_shill_properties(base::Value::Type::DICTIONARY);
  absl::optional<dbus::ObjectPath> service_path = ConfigureESimService(
      dbus::ObjectPath(kTestEuiccPath), profile_path, new_shill_properties);
  EXPECT_FALSE(service_path.has_value());
}

}  // namespace ash
