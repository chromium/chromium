// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "chromeos/ash/components/network/fake_network_connection_handler.h"
#include "chromeos/ash/services/cellular_setup/esim_test_base.h"
#include "chromeos/ash/services/cellular_setup/esim_test_utils.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-shared.h"
#include "components/user_manager/fake_user_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::cellular_setup {

namespace {

const char kProfileUninstallationResultHistogram[] =
    "Network.Cellular.ESim.ProfileUninstallationResult";
const char kProfileRenameResultHistogram[] =
    "Network.Cellular.ESim.ProfileRenameResult";
const char kPendingProfileLatencyHistogram[] =
    "Network.Cellular.ESim.ProfileDownload.PendingProfile.Latency";
const char kPendingProfileInstallHistogram[] =
    "Network.Cellular.ESim.InstallPendingProfile.Result";

class TestUserManager : public user_manager::FakeUserManager {
 public:
  explicit TestUserManager(bool is_guest) : is_guest_(is_guest) {
    user_manager::UserManager::SetInstance(this);
  }
  ~TestUserManager() override = default;

  // user_manager::UserManager:
  bool IsLoggedInAsGuest() const override { return is_guest_; }

 private:
  const bool is_guest_;
};

mojom::ESimOperationResult UninstallProfile(
    const mojo::Remote<mojom::ESimProfile>& esim_profile) {
  mojom::ESimOperationResult uninstall_result;

  base::RunLoop run_loop;
  esim_profile->UninstallProfile(base::BindOnce(
      [](mojom::ESimOperationResult* out_uninstall_result,
         base::OnceClosure quit_closure,
         mojom::ESimOperationResult uninstall_result) {
        *out_uninstall_result = uninstall_result;
        std::move(quit_closure).Run();
      },
      &uninstall_result, run_loop.QuitClosure()));
  run_loop.Run();
  return uninstall_result;
}

mojom::ESimOperationResult SetProfileNickname(
    const mojo::Remote<mojom::ESimProfile>& esim_profile,
    const std::u16string& nickname) {
  mojom::ESimOperationResult result;

  base::RunLoop run_loop;
  esim_profile->SetProfileNickname(
      nickname, base::BindOnce(
                    [](mojom::ESimOperationResult* out_result,
                       base::OnceClosure quit_closure,
                       mojom::ESimOperationResult result) {
                      *out_result = result;
                      std::move(quit_closure).Run();
                    },
                    &result, run_loop.QuitClosure()));
  run_loop.Run();
  return result;
}

}  // namespace

class ESimProfileTest : public ESimTestBase {
 public:
  ESimProfileTest() = default;
  ESimProfileTest(const ESimProfileTest&) = delete;
  ESimProfileTest& operator=(const ESimProfileTest&) = delete;

  void SetUp() override {
    ESimTestBase::SetUp();
    SetupEuicc();
  }

  void TearDown() override {
    HermesProfileClient::Get()->GetTestInterface()->SetEnableProfileBehavior(
        HermesProfileClient::TestInterface::EnableProfileBehavior::
            kConnectableButNotConnected);
  }

  void SetIsGuest(bool is_guest) {
    test_user_manager_ = std::make_unique<TestUserManager>(is_guest);
  }

  mojo::Remote<mojom::ESimProfile> GetESimProfileForIccid(
      const std::string& eid,
      const std::string& iccid) {
    mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(eid);
    if (!euicc.is_bound()) {
      return mojo::Remote<mojom::ESimProfile>();
    }
    std::vector<mojo::PendingRemote<mojom::ESimProfile>>
        profile_pending_remotes = GetProfileList(euicc);
    for (auto& profile_pending_remote : profile_pending_remotes) {
      mojo::Remote<mojom::ESimProfile> esim_profile(
          std::move(profile_pending_remote));
      mojom::ESimProfilePropertiesPtr profile_properties =
          GetESimProfileProperties(esim_profile);
      if (profile_properties->iccid == iccid) {
        return esim_profile;
      }
    }
    return mojo::Remote<mojom::ESimProfile>();
  }

  mojom::ProfileInstallResult InstallProfile(
      const mojo::Remote<mojom::ESimProfile>& esim_profile,
      bool wait_for_connect,
      bool fail_connect) {
    mojom::ProfileInstallResult out_install_result;

    base::RunLoop run_loop;
    esim_profile->InstallProfile(
        /*confirmation_code=*/std::string(),
        base::BindLambdaForTesting(
            [&](mojom::ProfileInstallResult install_result) {
              out_install_result = install_result;
              run_loop.Quit();
            }));

    FastForwardProfileRefreshDelay();
    FastForwardAutoConnectWaiting();

    if (wait_for_connect) {
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
    return out_install_result;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestUserManager> test_user_manager_;
};

TEST_F(ESimProfileTest, GetProperties) {
  SetIsGuest(false);

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(ESimTestBase::kTestEuiccPath),
      hermes::profile::State::kPending, "",
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  base::RunLoop().RunUntilIdle();
  HermesProfileClient::Properties* dbus_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);

  mojo::Remote<mojom::ESimProfile> esim_profile = GetESimProfileForIccid(
      ESimTestBase::kTestEid, dbus_properties->iccid().value());
  ASSERT_TRUE(esim_profile.is_bound());
  mojom::ESimProfilePropertiesPtr mojo_properties =
      GetESimProfileProperties(esim_profile);
  EXPECT_EQ(dbus_properties->iccid().value(), mojo_properties->iccid);
}

TEST_F(ESimProfileTest, InstallProfile) {
  SetIsGuest(false);
  base::HistogramTester histogram_tester;

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(ESimTestBase::kTestEuiccPath),
      hermes::profile::State::kPending, "",
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  base::RunLoop().RunUntilIdle();
  HermesProfileClient::Properties* dbus_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);

  // Verify that install errors return error code properly.
  euicc_test->QueueHermesErrorStatus(
      HermesResponseStatus::kErrorNeedConfirmationCode);
  mojo::Remote<mojom::ESimProfile> esim_profile = GetESimProfileForIccid(
      ESimTestBase::kTestEid, dbus_properties->iccid().value());
  ASSERT_TRUE(esim_profile.is_bound());
  mojom::ProfileInstallResult install_result = InstallProfile(
      esim_profile, /*wait_for_connect=*/false, /*fail_connect=*/false);
  EXPECT_EQ(mojom::ProfileInstallResult::kErrorNeedsConfirmationCode,
            install_result);

  histogram_tester.ExpectTotalCount(kPendingProfileLatencyHistogram, 0);
  histogram_tester.ExpectBucketCount(
      kPendingProfileInstallHistogram,
      HermesResponseStatus::kErrorNeedConfirmationCode,
      /*expected_count=*/1);

  // Adding a pending profile causes a list change.
  EXPECT_EQ(1u, observer()->profile_list_change_calls().size());

  // Verify that installing pending profile returns proper results
  // and updates esim_profile properties.
  install_result = InstallProfile(esim_profile, /*wait_for_connect=*/true,
                                  /*fail_connect=*/false);
  // Wait for property changes to propagate.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, install_result);
  mojom::ESimProfilePropertiesPtr mojo_properties =
      GetESimProfileProperties(esim_profile);
  EXPECT_EQ(dbus_properties->iccid().value(), mojo_properties->iccid);
  EXPECT_NE(mojo_properties->state, mojom::ProfileState::kPending);

  // Installing a profile causes a list change.
  EXPECT_EQ(2u, observer()->profile_list_change_calls().size());

  histogram_tester.ExpectTotalCount(kPendingProfileLatencyHistogram, 1);
  histogram_tester.ExpectBucketCount(kPendingProfileInstallHistogram,
                                     HermesResponseStatus::kSuccess,
                                     /*expected_count=*/1);
}

TEST_F(ESimProfileTest, InstallProfileAlreadyConnected) {
  SetIsGuest(false);

  dbus::ObjectPath profile_path =
      HermesEuiccClient::Get()->GetTestInterface()->AddFakeCarrierProfile(
          dbus::ObjectPath(ESimTestBase::kTestEuiccPath),
          hermes::profile::State::kPending, /*activation_code=*/std::string(),
          HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
              kAddProfileWithService);
  HermesProfileClient::Properties* dbus_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);
  mojo::Remote<mojom::ESimProfile> esim_profile = GetESimProfileForIccid(
      ESimTestBase::kTestEid, dbus_properties->iccid().value());

  HermesProfileClient::Get()->GetTestInterface()->SetEnableProfileBehavior(
      HermesProfileClient::TestInterface::EnableProfileBehavior::
          kConnectableAndConnected);

  mojom::ProfileInstallResult install_result =
      InstallProfile(esim_profile, /*wait_for_connect=*/false,
                     /*fail_connect=*/false);
  EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, install_result);
}

TEST_F(ESimProfileTest, InstallConnectFailure) {
  SetIsGuest(false);

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(ESimTestBase::kTestEuiccPath),
      hermes::profile::State::kPending, "",
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  base::RunLoop().RunUntilIdle();
  HermesProfileClient::Properties* dbus_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);
  mojo::Remote<mojom::ESimProfile> esim_profile = GetESimProfileForIccid(
      ESimTestBase::kTestEid, dbus_properties->iccid().value());

  // Verify that connect failures still return success code.
  mojom::ProfileInstallResult install_result =
      InstallProfile(esim_profile, /*wait_for_connect=*/true,
                     /*fail_connect=*/true);
  EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, install_result);
}

TEST_F(ESimProfileTest, UninstallProfile) {
  SetIsGuest(false);

  base::HistogramTester histogram_tester;

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
  EXPECT_EQ(1u, observer()->profile_list_change_calls().size());
  observer()->Reset();
  HermesProfileClient::Properties* pending_profile_dbus_properties =
      HermesProfileClient::Get()->GetProperties(pending_profile_path);
  HermesProfileClient::Properties* active_profile_dbus_properties =
      HermesProfileClient::Get()->GetProperties(active_profile_path);
  histogram_tester.ExpectTotalCount(kProfileUninstallationResultHistogram, 0);

  // Verify that uninstall error codes are returned properly.
  euicc_test->QueueHermesErrorStatus(
      HermesResponseStatus::kErrorInvalidResponse);
  mojo::Remote<mojom::ESimProfile> active_esim_profile = GetESimProfileForIccid(
      ESimTestBase::kTestEid, active_profile_dbus_properties->iccid().value());
  ASSERT_TRUE(active_esim_profile.is_bound());
  mojom::ESimOperationResult result = UninstallProfile(active_esim_profile);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kFailure, result);
  EXPECT_EQ(0u, observer()->profile_list_change_calls().size());
  histogram_tester.ExpectTotalCount(kProfileUninstallationResultHistogram, 1);
  histogram_tester.ExpectBucketCount(kProfileUninstallationResultHistogram,
                                     false, 1);

  // Verify that pending profiles cannot be uninstalled
  observer()->Reset();
  mojo::Remote<mojom::ESimProfile> pending_esim_profile =
      GetESimProfileForIccid(ESimTestBase::kTestEid,
                             pending_profile_dbus_properties->iccid().value());
  ASSERT_TRUE(pending_esim_profile.is_bound());
  result = UninstallProfile(pending_esim_profile);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kFailure, result);
  EXPECT_EQ(0u, observer()->profile_list_change_calls().size());
  histogram_tester.ExpectTotalCount(kProfileUninstallationResultHistogram, 1);
  histogram_tester.ExpectBucketCount(kProfileUninstallationResultHistogram,
                                     false, 1);

  // Verify that uninstall removes the profile and notifies observers properly.
  observer()->Reset();
  result = UninstallProfile(active_esim_profile);

  // The state change of the ESim profile from kActive to kInactive causes a
  // list change observer event.
  ASSERT_EQ(1u, observer()->profile_list_change_calls().size());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kSuccess, result);

  // The removal of the ESim profile causes a list change observer event.
  ASSERT_EQ(2u, observer()->profile_list_change_calls().size());

  EXPECT_EQ(1u, GetProfileList(GetEuiccForEid(ESimTestBase::kTestEid)).size());
  histogram_tester.ExpectTotalCount(kProfileUninstallationResultHistogram, 2);
  histogram_tester.ExpectBucketCount(kProfileUninstallationResultHistogram,
                                     true, 1);
}

TEST_F(ESimProfileTest, CannotUninstallProfileAsGuest) {
  SetIsGuest(true);

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath active_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "",
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  HermesProfileClient::Properties* active_profile_dbus_properties =
      HermesProfileClient::Get()->GetProperties(active_profile_path);
  mojo::Remote<mojom::ESimProfile> active_esim_profile = GetESimProfileForIccid(
      ESimTestBase::kTestEid, active_profile_dbus_properties->iccid().value());

  mojom::ESimOperationResult result = UninstallProfile(active_esim_profile);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kFailure, result);
}

TEST_F(ESimProfileTest, SetProfileNickName) {
  SetIsGuest(false);

  const std::u16string test_nickname = u"Test nickname";
  base::HistogramTester histogram_tester;

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
  EXPECT_EQ(1u, observer()->profile_list_change_calls().size());
  observer()->Reset();
  HermesProfileClient::Properties* pending_profile_dbus_properties =
      HermesProfileClient::Get()->GetProperties(pending_profile_path);
  HermesProfileClient::Properties* active_profile_dbus_properties =
      HermesProfileClient::Get()->GetProperties(active_profile_path);

  // Verify that pending profiles cannot be modified.
  mojo::Remote<mojom::ESimProfile> pending_esim_profile =
      GetESimProfileForIccid(ESimTestBase::kTestEid,
                             pending_profile_dbus_properties->iccid().value());
  ASSERT_TRUE(pending_esim_profile.is_bound());
  mojom::ESimOperationResult result =
      SetProfileNickname(pending_esim_profile, test_nickname);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kFailure, result);
  EXPECT_EQ(0u, observer()->profile_change_calls().size());

  // Verify that nickname can be set on active profiles.
  histogram_tester.ExpectTotalCount(kProfileRenameResultHistogram, 0);
  mojo::Remote<mojom::ESimProfile> active_esim_profile = GetESimProfileForIccid(
      ESimTestBase::kTestEid, active_profile_dbus_properties->iccid().value());
  ASSERT_TRUE(active_esim_profile.is_bound());
  result = SetProfileNickname(active_esim_profile, test_nickname);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kSuccess, result);
  histogram_tester.ExpectTotalCount(kProfileRenameResultHistogram, 1);
  histogram_tester.ExpectBucketCount(kProfileRenameResultHistogram, true, 1);

  mojom::ESimProfilePropertiesPtr active_profile_mojo_properties =
      GetESimProfileProperties(active_esim_profile);
  EXPECT_EQ(test_nickname, active_profile_mojo_properties->nickname);
}

TEST_F(ESimProfileTest, CannotSetProfileNickNameAsGuest) {
  SetIsGuest(true);

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath active_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "",
      HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
          kAddProfileWithService);
  HermesProfileClient::Properties* active_profile_dbus_properties =
      HermesProfileClient::Get()->GetProperties(active_profile_path);
  mojo::Remote<mojom::ESimProfile> active_esim_profile = GetESimProfileForIccid(
      ESimTestBase::kTestEid, active_profile_dbus_properties->iccid().value());

  mojom::ESimOperationResult result =
      SetProfileNickname(active_esim_profile, u"Nickname");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kFailure, result);
}

}  // namespace ash::cellular_setup
