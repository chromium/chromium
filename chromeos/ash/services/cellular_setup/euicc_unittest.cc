// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/euicc.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/network/fake_network_connection_handler.h"
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
      bool wait_for_create,
      bool wait_for_connect,
      bool fail_connect) {
    mojom::ProfileInstallResult out_install_result;
    mojo::PendingRemote<mojom::ESimProfile> out_esim_profile;

    base::RunLoop run_loop;
    euicc->InstallProfileFromActivationCode(
        activation_code, confirmation_code, /*is_install_via_qr_code=*/true,
        base::BindLambdaForTesting(
            [&](mojom::ProfileInstallResult install_result,
                mojo::PendingRemote<mojom::ESimProfile> esim_profile) {
              out_install_result = install_result;
              out_esim_profile = std::move(esim_profile);
              run_loop.Quit();
            }));

    if (wait_for_create) {
      base::RunLoop().RunUntilIdle();
      // Set the enable notify prolie list update back to true if it was false
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
      /*confirmation_code=*/std::string(), /*wait_for_create=*/true,
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, result_pair.first);
  EXPECT_TRUE(result_pair.second.is_valid());
  histogram_tester.ExpectBucketCount(kInstallViaQrCodeHistogram,
                                     HermesResponseStatus::kSuccess,
                                     /*expected_count=*/1);

  // Verify that install succeeds when valid activation code is passed.
  result_pair = InstallProfileFromActivationCode(
      euicc, euicc_test->GenerateFakeActivationCode(),
      /*confirmation_code=*/std::string(), /*wait_for_create=*/false,
      /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, result_pair.first);
  EXPECT_TRUE(result_pair.second.is_valid());
  histogram_tester.ExpectBucketCount(kInstallViaQrCodeHistogram,
                                     HermesResponseStatus::kSuccess,
                                     /*expected_count=*/2);
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
      /*confirmation_code=*/std::string(), /*wait_for_create=*/false,
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
      /*confirmation_code=*/std::string(), /*wait_for_create=*/false,
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

TEST_F(EuiccTest, RequestPendingProfiles) {
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

}  // namespace ash::cellular_setup
