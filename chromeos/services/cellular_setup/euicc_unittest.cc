// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/services/cellular_setup/esim_test_base.h"
#include "chromeos/services/cellular_setup/esim_test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {
namespace cellular_setup {

namespace {

using InstallResultPair = std::pair<mojom::ProfileInstallResult,
                                    mojo::PendingRemote<mojom::ESimProfile>>;

InstallResultPair InstallProfileFromActivationCode(
    const mojo::Remote<mojom::Euicc>& euicc,
    const std::string& activation_code,
    const std::string& confirmation_code) {
  mojom::ProfileInstallResult install_result;
  mojo::PendingRemote<mojom::ESimProfile> esim_profile;

  base::RunLoop run_loop;
  euicc->InstallProfileFromActivationCode(
      activation_code, confirmation_code,
      base::BindOnce(
          [](mojom::ProfileInstallResult* out_install_result,
             mojo::PendingRemote<mojom::ESimProfile>* out_esim_profile,
             base::OnceClosure quit_closure,
             mojom::ProfileInstallResult install_result,
             mojo::PendingRemote<mojom::ESimProfile> esim_profile) {
            *out_install_result = install_result;
            *out_esim_profile = std::move(esim_profile);
            std::move(quit_closure).Run();
          },
          &install_result, &esim_profile, run_loop.QuitClosure()));
  run_loop.Run();
  return std::make_pair(install_result, std::move(esim_profile));
}

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
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kActive, "");
  dbus::ObjectPath pending_profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();

  esim_profile_list = GetProfileList(euicc);
  EXPECT_EQ(2u, esim_profile_list.size());
}

TEST_F(EuiccTest, InstallProfileFromActivationCode) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  // Verify that install errors return error code properly.
  euicc_test->QueueHermesErrorStatus(
      HermesResponseStatus::kErrorInvalidActivationCode);
  InstallResultPair result_pair =
      InstallProfileFromActivationCode(euicc, "", "");
  EXPECT_EQ(mojom::ProfileInstallResult::kErrorInvalidActivationCode,
            result_pair.first);
  EXPECT_FALSE(result_pair.second.is_valid());

  // Verify that installing a profile returns proper status code
  // and profile object.
  dbus::ObjectPath profile_path = euicc_test->AddFakeCarrierProfile(
      dbus::ObjectPath(kTestEuiccPath), hermes::profile::State::kPending, "");
  base::RunLoop().RunUntilIdle();
  HermesProfileClient::Properties* dbus_properties =
      HermesProfileClient::Get()->GetProperties(profile_path);
  result_pair = InstallProfileFromActivationCode(
      euicc, dbus_properties->activation_code().value(), "");
  EXPECT_EQ(mojom::ProfileInstallResult::kSuccess, result_pair.first);
  ASSERT_TRUE(result_pair.second.is_valid());

  mojo::Remote<mojom::ESimProfile> esim_profile(std::move(result_pair.second));
  mojom::ESimProfilePropertiesPtr mojo_properties =
      GetESimProfileProperties(esim_profile);
  EXPECT_EQ(dbus_properties->iccid().value(), mojo_properties->iccid);
  EXPECT_EQ(3u, observer()->profile_list_change_calls().size());
}

TEST_F(EuiccTest, RequestPendingProfiles) {
  mojo::Remote<mojom::Euicc> euicc = GetEuiccForEid(ESimTestBase::kTestEid);
  ASSERT_TRUE(euicc.is_bound());

  HermesEuiccClient::TestInterface* euicc_test =
      HermesEuiccClient::Get()->GetTestInterface();
  // Verify that pending profile request errors are return properly.
  euicc_test->QueueHermesErrorStatus(HermesResponseStatus::kErrorNoResponse);
  EXPECT_EQ(mojom::ESimOperationResult::kFailure,
            RequestPendingProfiles(euicc));
  EXPECT_EQ(0u, observer()->profile_list_change_calls().size());

  // Verify that successful request returns correct status code.
  EXPECT_EQ(mojom::ESimOperationResult::kSuccess,
            RequestPendingProfiles(euicc));
}

}  // namespace cellular_setup
}  // namespace chromeos