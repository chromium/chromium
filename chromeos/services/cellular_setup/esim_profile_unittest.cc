// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/services/cellular_setup/esim_test_base.h"
#include "chromeos/services/cellular_setup/esim_test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {
namespace cellular_setup {

namespace {

mojom::ProfileInstallResult InstallProfile(
    const mojo::Remote<mojom::ESimProfile>& esim_profile,
    const std::string& confirmation_code) {
  mojom::ProfileInstallResult install_result;

  base::RunLoop run_loop;
  esim_profile->InstallProfile(
      confirmation_code, base::BindOnce(
                             [](mojom::ProfileInstallResult* out_install_result,
                                base::OnceClosure quit_closure,
                                mojom::ProfileInstallResult install_result) {
                               *out_install_result = install_result;
                               std::move(quit_closure).Run();
                             },
                             &install_result, run_loop.QuitClosure()));
  run_loop.Run();
  return install_result;
}

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

mojom::ESimOperationResult EnableProfile(
    const mojo::Remote<mojom::ESimProfile>& esim_profile) {
  mojom::ESimOperationResult enable_result;

  base::RunLoop run_loop;
  esim_profile->EnableProfile(base::BindOnce(
      [](mojom::ESimOperationResult* out_enable_result,
         base::OnceClosure quit_closure,
         mojom::ESimOperationResult enable_result) {
        *out_enable_result = enable_result;
        std::move(quit_closure).Run();
      },
      &enable_result, run_loop.QuitClosure()));
  run_loop.Run();
  return enable_result;
}

mojom::ESimOperationResult DisableProfile(
    const mojo::Remote<mojom::ESimProfile>& esim_profile) {
  mojom::ESimOperationResult disable_result;

  base::RunLoop run_loop;
  esim_profile->DisableProfile(base::BindOnce(
      [](mojom::ESimOperationResult* out_disable_result,
         base::OnceClosure quit_closure,
         mojom::ESimOperationResult disable_result) {
        *out_disable_result = disable_result;
        std::move(quit_closure).Run();
      },
      &disable_result, run_loop.QuitClosure()));
  run_loop.Run();
  return disable_result;
}

mojom::ESimOperationResult SetProfileNickname(
    const mojo::Remote<mojom::ESimProfile>& esim_profile,
    const base::string16& nickname) {
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
};

TEST_F(ESimProfileTest, GetProperties) {
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(ESimTestBase::kTestEuiccPath),
      hermes::profile::State::kPending, "");
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
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(ESimTestBase::kTestEuiccPath),
      hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  HermesProfileClient::Properties* dbus_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);

  // Verify that install errors return error code properly.
  euicc_test->QueueHermesErrorStatus(
      HermesResponseStatus::kErrorNeedConfirmationCode);
  mojo::Remote<mojom::ESimProfile> esim_profile = GetESimProfileForIccid(
      ESimTestBase::kTestEid, dbus_properties->iccid().value());
  ASSERT_TRUE(esim_profile.is_bound());
  mojom::ProfileInstallResult install_result = InstallProfile(esim_profile, "");
  EXPECT_EQ(mojom::ProfileInstallResult::kErrorNeedsConfirmationCode,
            install_result);

  // Verify that installing pending profile returns proper results
  // and updates esim_profile properties.
  install_result = InstallProfile(esim_profile, "");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, install_result);
  mojom::ESimProfilePropertiesPtr mojo_properties =
      GetESimProfileProperties(esim_profile);
  EXPECT_EQ(dbus_properties->iccid().value(), mojo_properties->iccid);
  EXPECT_NE(mojo_properties->state, mojom::ProfileState::kPending);
  EXPECT_EQ(3u, observer()->profile_list_change_calls().size());
}

TEST_F(ESimProfileTest, UninstallProfile) {
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath active_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "");
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, observer()->profile_list_change_calls().size());
  observer()->Reset();
  HermesProfileClient::Properties* pending_profile_dbus_properties =
      HermesProfileClient::Get()->GetProperties(pending_profile_path);
  HermesProfileClient::Properties* active_profile_dbus_properties =
      HermesProfileClient::Get()->GetProperties(active_profile_path);

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

  // Verify that uninstall removes the profile and notifies observers properly.
  observer()->Reset();
  result = UninstallProfile(active_esim_profile);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kSuccess, result);
  ASSERT_EQ(1u, observer()->profile_list_change_calls().size());
  EXPECT_EQ(1u, GetProfileList(GetEuiccForEid(ESimTestBase::kTestEid)).size());
}

TEST_F(ESimProfileTest, EnableProfile) {
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath inactive_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kInactive, "");
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, observer()->profile_list_change_calls().size());
  observer()->Reset();
  HermesProfileClient::Properties* pending_profile_dbus_properties =
      HermesProfileClient::Get()->GetProperties(pending_profile_path);
  HermesProfileClient::Properties* inactive_profile_dbus_properties =
      HermesProfileClient::Get()->GetProperties(inactive_profile_path);

  // Verify that pending profiles cannot be enabled.
  mojo::Remote<mojom::ESimProfile> pending_esim_profile =
      GetESimProfileForIccid(ESimTestBase::kTestEid,
                             pending_profile_dbus_properties->iccid().value());
  ASSERT_TRUE(pending_esim_profile.is_bound());
  mojom::ESimOperationResult result = EnableProfile(pending_esim_profile);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kFailure, result);
  EXPECT_EQ(0u, observer()->profile_change_calls().size());

  // Verify that enabling profile returns result properly.
  mojo::Remote<mojom::ESimProfile> inactive_esim_profile =
      GetESimProfileForIccid(ESimTestBase::kTestEid,
                             inactive_profile_dbus_properties->iccid().value());
  ASSERT_TRUE(inactive_esim_profile.is_bound());
  result = EnableProfile(inactive_esim_profile);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kSuccess, result);

  mojom::ESimProfilePropertiesPtr inactive_profile_mojo_properties =
      GetESimProfileProperties(inactive_esim_profile);
  EXPECT_EQ(inactive_profile_dbus_properties->iccid().value(),
            inactive_profile_mojo_properties->iccid);
  EXPECT_EQ(mojom::ProfileState::kActive,
            inactive_profile_mojo_properties->state);
}

TEST_F(ESimProfileTest, DisableProfile) {
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath active_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "");
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, observer()->profile_list_change_calls().size());
  observer()->Reset();
  HermesProfileClient::Properties* pending_profile_dbus_properties =
      HermesProfileClient::Get()->GetProperties(pending_profile_path);
  HermesProfileClient::Properties* active_profile_dbus_properties =
      HermesProfileClient::Get()->GetProperties(active_profile_path);

  // Verify that pending profiles cannot be disabled.
  mojo::Remote<mojom::ESimProfile> pending_esim_profile =
      GetESimProfileForIccid(ESimTestBase::kTestEid,
                             pending_profile_dbus_properties->iccid().value());
  ASSERT_TRUE(pending_esim_profile.is_bound());
  mojom::ESimOperationResult result = DisableProfile(pending_esim_profile);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kFailure, result);
  EXPECT_EQ(0u, observer()->profile_change_calls().size());

  // Verify that disabling profile returns result properly.
  mojo::Remote<mojom::ESimProfile> active_esim_profile = GetESimProfileForIccid(
      ESimTestBase::kTestEid, active_profile_dbus_properties->iccid().value());
  ASSERT_TRUE(active_esim_profile.is_bound());
  result = DisableProfile(active_esim_profile);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kSuccess, result);

  mojom::ESimProfilePropertiesPtr active_profile_mojo_properties =
      GetESimProfileProperties(active_esim_profile);
  EXPECT_EQ(active_profile_dbus_properties->iccid().value(),
            active_profile_mojo_properties->iccid);
  EXPECT_EQ(mojom::ProfileState::kInactive,
            active_profile_mojo_properties->state);
}

TEST_F(ESimProfileTest, SetProfileNickName) {
  const base::string16 test_nickname = base::UTF8ToUTF16("Test nickname");
  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  dbus::ObjectPath active_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "");
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, observer()->profile_list_change_calls().size());
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
  mojo::Remote<mojom::ESimProfile> active_esim_profile = GetESimProfileForIccid(
      ESimTestBase::kTestEid, active_profile_dbus_properties->iccid().value());
  ASSERT_TRUE(active_esim_profile.is_bound());
  result = SetProfileNickname(active_esim_profile, test_nickname);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(mojom::ESimOperationResult::kSuccess, result);

  mojom::ESimProfilePropertiesPtr active_profile_mojo_properties =
      GetESimProfileProperties(active_esim_profile);
  EXPECT_EQ(test_nickname, active_profile_mojo_properties->nickname);
}

}  // namespace cellular_setup
}  // namespace chromeos
