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
#include "base/test/scoped_feature_list.h"
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

const char kInstallViaQrCodeHistogram[] =
    "Network.Cellular.ESim.InstallViaQrCode.Result";

using InstallResultPair = std::pair<mojom::ProfileInstallResult,
                                    mojo::PendingRemote<mojom::ESimProfile>>;

mojom::ESimOperationResult RequestPendingProfiles(
    mojo::Remote<mojom::Euicc>& euicc) {
  mojom::ESimOperationResult result;
  base::RunLoop run_loop;
  euicc->RequestPendingProfiles(base::BindOnce(
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
  EuiccTest(const std::vector<base::test::FeatureRef>& enabled_features,
            const std::vector<base::test::FeatureRef>& disabled_features) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

class EuiccTest_SmdsSupportDisabled : public EuiccTest {
 public:
  EuiccTest_SmdsSupportDisabled(const EuiccTest_SmdsSupportDisabled&) = delete;
  EuiccTest_SmdsSupportDisabled& operator=(
      const EuiccTest_SmdsSupportDisabled&) = delete;

 protected:
  EuiccTest_SmdsSupportDisabled()
      : EuiccTest(
            /*enabled_features=*/{},
            /*disabled_features=*/{ash::features::kSmdsDbusMigration,
                                   ash::features::kSmdsSupport,
                                   ash::features::kSmdsSupportEuiccUpload}) {}
  ~EuiccTest_SmdsSupportDisabled() override = default;
};

class EuiccTest_SmdsSupportEnabled : public EuiccTest {
 public:
  EuiccTest_SmdsSupportEnabled(const EuiccTest_SmdsSupportEnabled&) = delete;
  EuiccTest_SmdsSupportEnabled& operator=(const EuiccTest_SmdsSupportEnabled&) =
      delete;

 protected:
  EuiccTest_SmdsSupportEnabled()
      : EuiccTest(
            /*enabled_features=*/{ash::features::kSmdsDbusMigration,
                                  ash::features::kSmdsSupport,
                                  ash::features::kSmdsSupportEuiccUpload},
            /*disabled_features=*/{}) {}
  ~EuiccTest_SmdsSupportEnabled() override = default;
};

TEST_F(EuiccTest_SmdsSupportDisabled, GetProperties) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());
  mojom::EuiccPropertiesPtr properties = GetEuiccProperties(euicc);
  EXPECT_EQ(ESimTestBase::kTestEid, properties->eid);
  EXPECT_EQ(true, properties->is_active);
}

TEST_F(EuiccTest_SmdsSupportDisabled, GetProfileList) {
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

TEST_F(EuiccTest_SmdsSupportDisabled, InstallProfileFromActivationCode) {
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
      /*install_method=*/mojom::ProfileInstallMethod::kViaQrCodeAfterSmds,
      /*wait_for_create=*/true,
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, result_pair.first);
  EXPECT_TRUE(result_pair.second.is_valid());
  histogram_tester.ExpectBucketCount(kInstallViaQrCodeHistogram,
                                     HermesResponseStatus::kSuccess,
                                     /*expected_count=*/1);

  // Verify that install succeeds when valid activation code is passed.
  result_pair = InstallProfileFromActivationCode(
      euicc, euicc_test->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(),
      /*install_method=*/mojom::ProfileInstallMethod::kViaQrCodeAfterSmds,
      /*wait_for_create=*/false,
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, result_pair.first);
  EXPECT_TRUE(result_pair.second.is_valid());
  histogram_tester.ExpectBucketCount(kInstallViaQrCodeHistogram,
                                     HermesResponseStatus::kSuccess,
                                     /*expected_count=*/2);
}

TEST_F(EuiccTest_SmdsSupportDisabled, InstallProfileAlreadyConnected) {
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

TEST_F(EuiccTest_SmdsSupportDisabled, InstallPendingProfileFromActivationCode) {
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
  EXPECT_EQ(2u, observer()->profile_list_change_calls().size());
}

TEST_F(EuiccTest_SmdsSupportDisabled, RequestPendingProfiles) {
  static const char kOperationResultMetric[] =
      "Network.Cellular.ESim.RequestPendingProfiles.OperationResult";
  base::HistogramTester histogram_tester;

  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  // Verify that pending profile request errors are return properly.
  euicc_test->QueueHermesErrorStatus(HermesResponseStatus::kErrorNoResponse);
  EXPECT_EQ(mojom::ESimOperationResult::kFailure,
            RequestPendingProfiles(euicc));
  histogram_tester.ExpectBucketCount(
      kOperationResultMetric,
      Euicc::RequestPendingProfilesResult::kInhibitFailed,
      /*expected_count=*/1);
  EXPECT_EQ(0u, observer()->profile_list_change_calls().size());

  constexpr base::TimeDelta kHermesInteractiveDelay = base::Milliseconds(3000);
  HermesEuiccClient::Get()->GetTestInterface()->SetInteractiveDelay(
      kHermesInteractiveDelay);

  // Verify that successful request returns correct status code.
  EXPECT_EQ(mojom::ESimOperationResult::kSuccess,
            RequestPendingProfiles(euicc));

  // Before requesting pending profiles, we request installed profiles, so we
  // expect that there will be 2 delays (one for installed, one for pending).
  histogram_tester.ExpectTimeBucketCount(
      "Network.Cellular.ESim.ProfileDiscovery.Latency",
      2 * kHermesInteractiveDelay, 1);
  histogram_tester.ExpectTotalCount(
      "Network.Cellular.ESim.ProfileDiscovery.Latency", 1);
  histogram_tester.ExpectBucketCount(
      kOperationResultMetric, Euicc::RequestPendingProfilesResult::kSuccess,
      /*expected_count=*/1);
}

TEST_F(EuiccTest_SmdsSupportDisabled, GetEidQRCode) {
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

TEST_F(EuiccTest_SmdsSupportDisabled, RequestAvailableProfiles) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::features::kSmdsDbusMigration, ash::features::kSmdsSupport,
       ash::features::kSmdsSupportEuiccUpload},
      {});

  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  absl::optional<mojom::ESimOperationResult> result;
  absl::optional<std::vector<mojom::ESimProfilePropertiesPtr>>
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

TEST_F(EuiccTest_SmdsSupportDisabled, RequestAvailableProfiles_FailToInhibit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::features::kSmdsDbusMigration, ash::features::kSmdsSupport,
       ash::features::kSmdsSupportEuiccUpload},
      {});

  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  // The cellular device is inhibited by setting a device property. Simulate a
  // failure to inhibit by making the next attempt to set a property fail.
  SetErrorForNextSetPropertyAttempt("error_name");

  absl::optional<mojom::ESimOperationResult> result;
  absl::optional<std::vector<mojom::ESimProfilePropertiesPtr>>
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

TEST_F(EuiccTest_SmdsSupportEnabled, GetProperties) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());
  mojom::EuiccPropertiesPtr properties = GetEuiccProperties(euicc);
  EXPECT_EQ(ESimTestBase::kTestEid, properties->eid);
  EXPECT_EQ(true, properties->is_active);
}

TEST_F(EuiccTest_SmdsSupportEnabled, GetProfileList) {
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

TEST_F(EuiccTest_SmdsSupportEnabled, InstallProfileFromActivationCode) {
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

TEST_F(EuiccTest_SmdsSupportEnabled, InstallProfileAlreadyConnected) {
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

TEST_F(EuiccTest_SmdsSupportEnabled, InstallPendingProfileFromActivationCode) {
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
  EXPECT_EQ(2u, observer()->profile_list_change_calls().size());
}

TEST_F(EuiccTest_SmdsSupportEnabled, RequestPendingProfiles) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  // Verify that pending profile request errors are return properly.
  euicc_test->QueueHermesErrorStatus(HermesResponseStatus::kErrorNoResponse);
  EXPECT_EQ(mojom::ESimOperationResult::kFailure,
            RequestPendingProfiles(euicc));
  EXPECT_EQ(0u, observer()->profile_list_change_calls().size());

  constexpr base::TimeDelta kHermesInteractiveDelay = base::Milliseconds(3000);
  HermesEuiccClient::Get()->GetTestInterface()->SetInteractiveDelay(
      kHermesInteractiveDelay);

  // Verify that successful request returns correct status code.
  EXPECT_EQ(mojom::ESimOperationResult::kSuccess,
            RequestPendingProfiles(euicc));
}

TEST_F(EuiccTest_SmdsSupportEnabled, GetEidQRCode) {
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

TEST_F(EuiccTest_SmdsSupportEnabled, RequestAvailableProfiles) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::features::kSmdsDbusMigration, ash::features::kSmdsSupport,
       ash::features::kSmdsSupportEuiccUpload},
      {});

  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  absl::optional<mojom::ESimOperationResult> result;
  absl::optional<std::vector<mojom::ESimProfilePropertiesPtr>>
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

TEST_F(EuiccTest_SmdsSupportEnabled, RequestAvailableProfiles_FailToInhibit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::features::kSmdsDbusMigration, ash::features::kSmdsSupport,
       ash::features::kSmdsSupportEuiccUpload},
      {});

  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  // The cellular device is inhibited by setting a device property. Simulate a
  // failure to inhibit by making the next attempt to set a property fail.
  SetErrorForNextSetPropertyAttempt("error_name");

  absl::optional<mojom::ESimOperationResult> result;
  absl::optional<std::vector<mojom::ESimProfilePropertiesPtr>>
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

}  // namespace ash::cellular_setup
