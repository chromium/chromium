// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cellular_esim_installer.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/hermes/hermes_clients.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/network/cellular_connection_handler.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/fake_network_connection_handler.h"
#include "chromeos/network/fake_stub_cellular_networks_provider.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/test_cellular_esim_profile_handler.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using InstallResultPair =
    std::pair<HermesResponseStatus, absl::optional<dbus::ObjectPath>>;

const char kTestEuiccPath[] = "/org/chromium/Hermes/Euicc/0";
const char kTestEid[] = "12345678901234567890123456789012";
const char kInstallViaQrCodeHistogram[] =
    "Network.Cellular.ESim.InstallViaQrCode.Result";
const char kInstallViaQrCodeOperationHistogram[] =
    "Network.Cellular.ESim.InstallViaQrCode.OperationResult";
const char kESimProfileDownloadLatencyHistogram[] =
    "Network.Cellular.ESim.ProfileDownload.ActivationCode.Latency";

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
    network_device_handler_.reset();
    network_state_handler_.reset();
    network_connection_handler_.reset();

    hermes_clients::Shutdown();
    shill_clients::Shutdown();
  }

  InstallResultPair InstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      const dbus::ObjectPath euicc_path,
      bool wait_for_connect,
      bool fail_connect) {
    HermesResponseStatus out_install_result;
    absl::optional<dbus::ObjectPath> out_esim_profile_path;

    base::RunLoop run_loop;
    cellular_esim_installer_->InstallProfileFromActivationCode(
        activation_code, confirmation_code, euicc_path,
        base::BindLambdaForTesting(
            [&](HermesResponseStatus install_result,
                absl::optional<dbus::ObjectPath> esim_profile_path) {
              out_install_result = install_result;
              out_esim_profile_path = esim_profile_path;
              run_loop.Quit();
            }));

    FastForwardProfileRefreshDelay();

    if (wait_for_connect) {
      base::RunLoop().RunUntilIdle();
      EXPECT_LE(1u, network_connection_handler_->connect_calls().size());
      if (fail_connect) {
        network_connection_handler_->connect_calls().back().InvokeErrorCallback(
            "fake_error_name", /*error_data=*/nullptr);
      } else {
        network_connection_handler_->connect_calls()
            .back()
            .InvokeSuccessCallback();
      }
    }

    run_loop.Run();
    return std::make_pair(out_install_result, out_esim_profile_path);
  }

  void FastForwardProfileRefreshDelay() {
    const base::TimeDelta kProfileRefreshCallbackDelay =
        base::TimeDelta::FromMilliseconds(150);

    // Connect can result in two profile refresh calls before and after
    // enabling profile. Fast forward by delay after refresh.
    task_environment_.FastForwardBy(2 * kProfileRefreshCallbackDelay);
  }

  base::HistogramTester* HistogramTesterPtr() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
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
  std::unique_ptr<FakeStubCellularNetworksProvider>
      stub_cellular_networks_provider_;
};

TEST_F(CellularESimInstallerTest, InstallProfileInvalidActivationCode) {
  InstallResultPair result_pair = InstallProfileFromActivationCode(
      /*activation_code=*/std::string(), /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(HermesResponseStatus::kErrorInvalidActivationCode,
            result_pair.first);
  EXPECT_EQ(result_pair.second, absl::nullopt);

  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaQrCodeHistogram,
      HermesResponseStatus::kErrorInvalidActivationCode,
      /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaQrCodeOperationHistogram,
      CellularESimInstaller::InstallProfileViaQrCodeResult::
          kHermesInstallFailed,
      /*expected_count=*/1);
}

TEST_F(CellularESimInstallerTest, InstallProfileConnectFailure) {
  // Verify that connect failures are handled properly.
  InstallResultPair result_pair = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*wait_for_connect=*/true, /*fail_connect=*/true);
  EXPECT_EQ(HermesResponseStatus::kSuccess, result_pair.first);
  EXPECT_NE(result_pair.second, absl::nullopt);
  HistogramTesterPtr()->ExpectBucketCount(kInstallViaQrCodeHistogram,
                                          HermesResponseStatus::kSuccess,
                                          /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaQrCodeOperationHistogram,
      CellularESimInstaller::InstallProfileViaQrCodeResult::kSuccess,
      /*expected_count=*/1);
}

TEST_F(CellularESimInstallerTest, InstallProfileSuccess) {
  // Verify that install succeeds when valid activation code is passed.
  InstallResultPair result_pair = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*wait_for_connect=*/true, /*fail_connect=*/false);
  EXPECT_EQ(HermesResponseStatus::kSuccess, result_pair.first);
  EXPECT_NE(result_pair.second, absl::nullopt);

  HistogramTesterPtr()->ExpectTotalCount(kESimProfileDownloadLatencyHistogram,
                                         1);
  HistogramTesterPtr()->ExpectBucketCount(kInstallViaQrCodeHistogram,
                                          HermesResponseStatus::kSuccess,
                                          /*expected_count=*/1);
  HistogramTesterPtr()->ExpectBucketCount(
      kInstallViaQrCodeOperationHistogram,
      CellularESimInstaller::InstallProfileViaQrCodeResult::kSuccess,
      /*expected_count=*/1);
}

TEST_F(CellularESimInstallerTest, InstallProfileAlreadyConnected) {
  HermesProfileClient::Get()->GetTestInterface()->SetEnableProfileBehavior(
      HermesProfileClient::TestInterface::EnableProfileBehavior::
          kConnectableAndConnected);

  InstallResultPair result_pair = InstallProfileFromActivationCode(
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*euicc_path=*/dbus::ObjectPath(kTestEuiccPath),
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(HermesResponseStatus::kSuccess, result_pair.first);
  EXPECT_NE(result_pair.second, absl::nullopt);
}

}  // namespace chromeos