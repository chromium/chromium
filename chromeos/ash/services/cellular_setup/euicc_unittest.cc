// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/euicc.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/fake_network_connection_handler.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/test_cellular_esim_profile_handler.h"
#include "chromeos/ash/services/cellular_setup/esim_test_base.h"
#include "chromeos/ash/services/cellular_setup/esim_test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::cellular_setup {

namespace {

using InstallResultPair = std::pair<mojom::ProfileInstallResult,
                                    mojo::PendingRemote<mojom::ESimProfile>>;

mojom::ESimOperationResult RefreshInstalledProfiles(
    mojo::Remote<mojom::Euicc>& euicc) {
  mojom::ESimOperationResult result;
  base::RunLoop run_loop;
  euicc->RefreshInstalledProfiles(base::BindOnce(
      [](mojom::ESimOperationResult* out, base::OnceClosure quit_closure,
         mojom::ESimOperationResult result) {
        *out = result;
        std::move(quit_closure).Run();
      },
      &result, run_loop.QuitClosure()));
  run_loop.Run();
  return result;
}

}  // namespace

class EuiccTest : public ESimTestBase {
 public:
  EuiccTest() = default;
  EuiccTest(const EuiccTest&) = delete;
  EuiccTest& operator=(const EuiccTest&) = delete;

  void SetUp() override {
    ESimTestBase::SetUp();
    SetupEuicc();
  }

  void TearDown() override {
    HermesProfileClient::Get()->GetTestInterface()->SetEnableProfileBehavior(
        HermesProfileClient::TestInterface::EnableProfileBehavior::
            kConnectableButNotConnected);
  }

  InstallResultPair InstallProfileFromActivationCode(
      const mojo::Remote<mojom::Euicc>& euicc,
      const std::string& activation_code,
      const std::string& confirmation_code,
      mojom::ProfileInstallMethod install_method,
      bool wait_for_create,
      bool wait_for_connect,
      bool fail_connect) {
    mojom::ProfileInstallResult out_install_result;
    mojo::PendingRemote<mojom::ESimProfile> out_esim_profile;

    base::RunLoop run_loop;
    euicc->InstallProfileFromActivationCode(
        activation_code, confirmation_code, install_method,
        base::BindLambdaForTesting(
            [&](mojom::ProfileInstallResult install_result,
                mojo::PendingRemote<mojom::ESimProfile> esim_profile) {
              out_install_result = install_result;
              out_esim_profile = std::move(esim_profile);
              run_loop.Quit();
            }));

    if (wait_for_create) {
      base::RunLoop().RunUntilIdle();
      // Set the enable notify profile list update back to true if it was false
      // before and fire the pending notify profile list update and that should
      // make the pending create callback complete.
      cellular_esim_profile_handler()->SetEnableNotifyProfileListUpdate(true);
    }
    FastForwardProfileRefreshDelay();

    if (wait_for_connect) {
      FastForwardAutoConnectWaiting();
      base::RunLoop().RunUntilIdle();
      EXPECT_LE(1u, network_connection_handler()->connect_calls().size());
      if (fail_connect) {
        network_connection_handler()
            ->connect_calls()
            .back()
            .InvokeErrorCallback("fake_error_name");
      } else {
        network_connection_handler()
            ->connect_calls()
            .back()
            .InvokeSuccessCallback();
      }
    }

    run_loop.Run();
    return std::make_pair(out_install_result, std::move(out_esim_profile));
  }

  void SetErrorForNextSetPropertyAttempt(const std::string& error_name) {
    ShillDeviceClient::Get()
        ->GetTestInterface()
        ->SetErrorForNextSetPropertyAttempt(error_name);
    base::RunLoop().RunUntilIdle();
  }

};

TEST_F(EuiccTest, GetProperties) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());
  mojom::EuiccPropertiesPtr properties = GetEuiccProperties(euicc);
  EXPECT_EQ(ESimTestBase::kTestEid, properties->eid);
  EXPECT_EQ(true, properties->is_active);
}

TEST_F(EuiccTest, GetProfileList) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());
  std::vector<mojo::PendingRemote<mojom::ESimProfile>> esim_profile_list =
      GetProfileList(euicc);
  EXPECT_EQ(0u, esim_profile_list.size());

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath active_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "",
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "",
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  base::RunLoop().RunUntilIdle();

  esim_profile_list = GetProfileList(euicc);
  EXPECT_EQ(2u, esim_profile_list.size());
}

TEST_F(EuiccTest, InstallProfileFromActivationCode) {
  base::HistogramTester histogram_tester;
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  // Verify that the callback is not completed if update profile list
  // notification is not fired and save the callback until an update profile
  // list gets called.
  cellular_esim_profile_handler()->SetEnableNotifyProfileListUpdate(false);
  InstallResultPair result_pair = InstallProfileFromActivationCode(
      euicc, euicc_test->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*install_method=*/mojom::ProfileInstallMethod::kViaSmds,
      /*wait_for_create=*/true,
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, result_pair.first);
  EXPECT_TRUE(result_pair.second.is_valid());
  histogram_tester.ExpectBucketCount(
      CellularNetworkMetricsLogger::kESimUserInstallMethod,
      CellularNetworkMetricsLogger::ESimUserInstallMethod::kViaSmds,
      /*expected_count=*/1);

  size_t expected_total_count = 1;
  histogram_tester.ExpectTotalCount(
      CellularNetworkMetricsLogger::kESimUserInstallMethod,
      expected_total_count);

  const std::vector<
      std::pair<mojom::ProfileInstallMethod,
                CellularNetworkMetricsLogger::ESimUserInstallMethod>>
      kInstallMethodPairs = {
          {
              mojom::ProfileInstallMethod::kViaQrCodeAfterSmds,
              CellularNetworkMetricsLogger::ESimUserInstallMethod::
                  kViaQrCodeAfterSmds,
          },
          {
              mojom::ProfileInstallMethod::kViaQrCodeSkippedSmds,
              CellularNetworkMetricsLogger::ESimUserInstallMethod::
                  kViaQrCodeSkippedSmds,
          },
          {
              mojom::ProfileInstallMethod::kViaActivationCodeAfterSmds,
              CellularNetworkMetricsLogger::ESimUserInstallMethod::
                  kViaActivationCodeAfterSmds,
          },
          {
              mojom::ProfileInstallMethod::kViaActivationCodeSkippedSmds,
              CellularNetworkMetricsLogger::ESimUserInstallMethod::
                  kViaActivationCodeSkippedSmds,
          },
      };

  for (const auto& install_method_pair : kInstallMethodPairs) {
    // Verify that install succeeds when valid activation code is passed.
    result_pair = InstallProfileFromActivationCode(
        euicc, euicc_test->GenerateFakeActivationCode(),
        /*confirmation_code=*/std::string(),
        /*install_method=*/install_method_pair.first, /*wait_for_create=*/false,
        /*wait_for_connect=*/false, /*fail_connect=*/false);
    EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, result_pair.first);
    EXPECT_TRUE(result_pair.second.is_valid());

    histogram_tester.ExpectBucketCount(
        CellularNetworkMetricsLogger::kESimUserInstallMethod,
        install_method_pair.second,
        /*expected_count=*/1);
    histogram_tester.ExpectTotalCount(
        CellularNetworkMetricsLogger::kESimUserInstallMethod,
        ++expected_total_count);
  }
}

TEST_F(EuiccTest, InstallProfileAlreadyConnected) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  HermesProfileClient::Get()->GetTestInterface()->SetEnableProfileBehavior(
      HermesProfileClient::TestInterface::EnableProfileBehavior::
          kConnectableAndConnected);

  InstallResultPair result_pair = InstallProfileFromActivationCode(
      euicc,
      HermesEuiccClient::Get()
          ->GetTestInterface()
          ->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*install_method=*/mojom::ProfileInstallMethod::kViaQrCodeAfterSmds,
      /*wait_for_create=*/false,
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, result_pair.first);
}

TEST_F(EuiccTest, InstallPendingProfileFromActivationCode) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  // Verify that installing a pending profile with its activation code returns
  // proper status code and profile object.
  dbus::ObjectPath profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "",
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  base::RunLoop().RunUntilIdle();
  HermesProfileClient::Properties* dbus_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);

  // Adding a pending profile causes a list change.
  EXPECT_EQ(1u, observer()->profile_list_change_calls().size());

  InstallResultPair result_pair = InstallProfileFromActivationCode(
      euicc, dbus_properties->activation_code().value(),
      /*confirmation_code=*/std::string(),
      /*install_method=*/mojom::ProfileInstallMethod::kViaQrCodeAfterSmds,
      /*wait_for_create=*/false,
      /*wait_for_connect=*/true, /*fail_connect=*/false);
  EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, result_pair.first);
  ASSERT_TRUE(result_pair.second.is_valid());

  mojo::Remote<mojom::ESimProfile> esim_profile(std::move(result_pair.second));
  mojom::ESimProfilePropertiesPtr mojo_properties =
      GetESimProfileProperties(esim_profile);
  EXPECT_EQ(dbus_properties->iccid().value(), mojo_properties->iccid);

  // Installing a profile causes a list change.
  EXPECT_EQ(3u, observer()->profile_list_change_calls().size());
}

TEST_F(EuiccTest, GetEidQRCode) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  mojom::QRCodePtr qr_code_result;
  base::RunLoop run_loop;
  euicc->GetEidQRCode(base::BindOnce(
      [](mojom::QRCodePtr* out, base::OnceClosure quit_closure,
         mojom::QRCodePtr properties) {
        *out = std::move(properties);
        std::move(quit_closure).Run();
      },
      &qr_code_result, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_FALSE(qr_code_result.is_null());
  EXPECT_LT(0, qr_code_result->size);
}

TEST_F(EuiccTest, RequestAvailableProfiles) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  std::optional<mojom::ESimOperationResult> result;
  std::optional<std::vector<mojom::ESimProfilePropertiesPtr>>
      profile_properties_list;

  base::RunLoop run_loop;
  euicc->RequestAvailableProfiles(base::BindLambdaForTesting(
      [&](mojom::ESimOperationResult returned_result,
          std::vector<mojom::ESimProfilePropertiesPtr>
              returned_profile_properties_list) {
        result = returned_result;
        profile_properties_list = std::move(returned_profile_properties_list);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, mojom::ESimOperationResult::kSuccess);

  const std::vector<std::string> smds_activation_codes =
      cellular_utils::GetSmdsActivationCodes();

  ASSERT_TRUE(profile_properties_list.has_value());
  EXPECT_EQ(smds_activation_codes.size(), profile_properties_list->size());

  for (const auto& profile_properties : *profile_properties_list) {
    EXPECT_EQ(profile_properties->eid, GetEuiccProperties(euicc)->eid);
    EXPECT_NE(
        std::find(smds_activation_codes.begin(), smds_activation_codes.end(),
                  profile_properties->activation_code),
        smds_activation_codes.end());
  }
}

TEST_F(EuiccTest, RequestAvailableProfiles_FailToInhibit) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  // The cellular device is inhibited by setting a device property. Simulate a
  // failure to inhibit by making the next attempt to set a property fail.
  SetErrorForNextSetPropertyAttempt("error_name");

  std::optional<mojom::ESimOperationResult> result;
  std::optional<std::vector<mojom::ESimProfilePropertiesPtr>>
      profile_properties_list;

  {
    base::RunLoop run_loop;
    euicc->RequestAvailableProfiles(base::BindLambdaForTesting(
        [&](mojom::ESimOperationResult returned_result,
            std::vector<mojom::ESimProfilePropertiesPtr>
                returned_profile_properties_list) {
          result = returned_result;
          profile_properties_list = std::move(returned_profile_properties_list);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, mojom::ESimOperationResult::kFailure);

  ASSERT_TRUE(profile_properties_list.has_value());
  EXPECT_TRUE(profile_properties_list->empty());

  {
    base::RunLoop run_loop;
    euicc->RequestAvailableProfiles(base::BindLambdaForTesting(
        [&](mojom::ESimOperationResult returned_result,
            std::vector<mojom::ESimProfilePropertiesPtr>
                returned_profile_properties_list) {
          result = returned_result;
          profile_properties_list = std::move(returned_profile_properties_list);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  EXPECT_EQ(*result, mojom::ESimOperationResult::kSuccess);

  const std::vector<std::string> smds_activation_codes =
      cellular_utils::GetSmdsActivationCodes();

  EXPECT_EQ(smds_activation_codes.size(), profile_properties_list->size());

  for (const auto& profile_properties : *profile_properties_list) {
    EXPECT_EQ(profile_properties->eid, GetEuiccProperties(euicc)->eid);
    EXPECT_NE(
        std::find(smds_activation_codes.begin(), smds_activation_codes.end(),
                  profile_properties->activation_code),
        smds_activation_codes.end());
  }
}

TEST_F(EuiccTest, RefreshInstalledProfiles) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();

  // Failing to inhibit the cellular device will cause the refresh attempt to
  // fail.
  SetErrorForNextSetPropertyAttempt("error_name");
  EXPECT_EQ(mojom::ESimOperationResult::kFailure,
            RefreshInstalledProfiles(euicc));

  euicc_test->QueueHermesErrorStatus(HermesResponseStatus::kErrorNoResponse);
  EXPECT_EQ(mojom::ESimOperationResult::kFailure,
            RefreshInstalledProfiles(euicc));

  EXPECT_EQ(mojom::ESimOperationResult::kSuccess,
            RefreshInstalledProfiles(euicc));
}

}  // namespace ash::cellular_setup
