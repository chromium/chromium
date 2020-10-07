// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/gaiacp/device_policies_manager.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class GcpDevicePoliciesBaseTest : public GlsRunnerTestBase {
 protected:
  void SetUp() override;
};

void GcpDevicePoliciesBaseTest::SetUp() {
  GlsRunnerTestBase::SetUp();

  // Remove the mdm_url value which exists by default as it's added in
  // InitializeRegistryOverrideForTesting.
  base::win::RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_WRITE));
  EXPECT_EQ(ERROR_SUCCESS, key.DeleteValue(kRegMdmUrl));
}

TEST_F(GcpDevicePoliciesBaseTest, NewUserAssociationWithNoUserPoliciesPresent) {
  FakeUserPoliciesManager fake_user_policies_manager(true);

  // Create a few fake users associated to fake gaia ids.
  std::vector<base::string16> sids;
  const size_t num_users_needed = 3;
  for (size_t i = 0; i < num_users_needed; ++i) {
    CComBSTR sid_str;
    base::string16 username = L"new-user-" + base::NumberToString16(i);
    base::string16 gaia_id = L"gaia-id-" + base::NumberToString16(i);
    base::string16 email = base::StringPrintf(L"user_%d@company.com", i);
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        username, L"password", L"Full Name", L"comment",
                        gaia_id, email, &sid_str));
    sids.push_back(OLE2W(sid_str));
  }

  // Create an existing user association in registry but with an invalid sid.
  base::win::RegKey key;
  base::string16 key_name = base::StringPrintf(
      L"%ls\\%ls", kGcpUsersRootKeyName, L"non-existent-user-sid");
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS,
            key.WriteValue(L"email", L"invalid-user@company.com"));

  // Add user cloud policies only for the first two users.
  UserPolicies first_user_policy;
  first_user_policy.enable_dm_enrollment = false;
  first_user_policy.enable_gcpw_auto_update = false;
  first_user_policy.enable_multi_user_login = false;
  first_user_policy.gcpw_pinned_version = GcpwVersion("100.1.2.3");

  fake_user_policies_manager.SetUserPolicies(sids[0], first_user_policy);

  UserPolicies second_user_policy = first_user_policy;
  second_user_policy.enable_dm_enrollment = true;
  second_user_policy.gcpw_pinned_version = GcpwVersion("102.1.2.4");
  fake_user_policies_manager.SetUserPolicies(sids[1], second_user_policy);

  // Create a device policy by merging the two users with cloud policies.
  DevicePolicies merged_device_policy =
      DevicePolicies::FromUserPolicies(first_user_policy);
  merged_device_policy.MergeWith(
      DevicePolicies::FromUserPolicies(second_user_policy));

  // Get the resolved device policy.
  DevicePolicies device_policy;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policy);

  // The resolved policy should reflect only the policies of the users with
  // existing cloud policies.
  ASSERT_EQ(merged_device_policy, device_policy);
}

// Tests that existing registry values that control device policies are honored
// correctly when present.
// Parameters are:
// 1. int  0: "enable_dm_enrollment" flag is set to 0.
//         1: "enable_dm_enrollment" flag is set to 1.
//         2: "enable_dm_enrollment" flag is not set.
// 2. int  0: "mdm" flag for MDM url is set to "".
//         1: "mdm" flag for MDM url is set to some valid value.
//         2: "mdm" flag for MDM url is not set.
// 3. int  0: "enable_multi_user_login" flag is set to 0.
//         1: "enable_multi_user_login" flag is set to 1.
//         2: "enable_multi_user_login" flag is not set.
class GcpDevicePoliciesRegistryTest
    : public GcpDevicePoliciesBaseTest,
      public ::testing::WithParamInterface<std::tuple<int, int, int>> {};

TEST_P(GcpDevicePoliciesRegistryTest, DefaultValues) {
  int dm_enrollment_flag = std::get<0>(GetParam());
  int mdm_url_flag = std::get<1>(GetParam());
  int multi_user_login_flag = std::get<2>(GetParam());

  FakeDevicePoliciesManager fake_device_policies_manager(true);

  if (dm_enrollment_flag < 2) {
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment,
                                            dm_enrollment_flag ? 1 : 0));
  }

  if (mdm_url_flag == 1) {
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  } else if (mdm_url_flag == 0) {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Open(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_WRITE));
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kRegMdmUrl, L""));
  }

  if (multi_user_login_flag < 2) {
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser,
                                            multi_user_login_flag ? 1 : 0));
  }

  DevicePolicies default_device_policies;

  // Enabled unless explicitly forbidden through setting either registry flags.
  bool enable_dm_enrollment = !(!dm_enrollment_flag || !mdm_url_flag);

  ASSERT_EQ(enable_dm_enrollment, default_device_policies.enable_dm_enrollment);
  ASSERT_EQ(multi_user_login_flag > 0,
            default_device_policies.enable_multi_user_login);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpDevicePoliciesRegistryTest,
                         ::testing::Combine(::testing::Values(0, 1, 2),
                                            ::testing::Values(0, 1, 2),
                                            ::testing::Values(0, 1, 2)));

// Tests that the merging of two device policies does not lead to a more
// restrictive policy.
// Parameters are:
// 1. bool : Whether MDM enrollment is enabled.
// 2. bool : Whether GCPW auto update through Omaha is enabled.
// 3. bool : Whether multi user mode is enabled.
// 4. string : Version of GCPW to use.
class GcpDevicePoliciesMergeTest
    : public GcpDevicePoliciesBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, bool, bool, const char*>> {};

TEST_P(GcpDevicePoliciesMergeTest, OtherUser) {
  UserPolicies new_user_policy;
  new_user_policy.enable_dm_enrollment = std::get<0>(GetParam());
  new_user_policy.enable_gcpw_auto_update = std::get<1>(GetParam());
  new_user_policy.enable_multi_user_login = std::get<2>(GetParam());
  new_user_policy.gcpw_pinned_version = GcpwVersion(std::get<3>(GetParam()));

  UserPolicies existing_user_policy;
  existing_user_policy.enable_dm_enrollment = true;
  existing_user_policy.enable_gcpw_auto_update = true;
  existing_user_policy.enable_multi_user_login = true;
  existing_user_policy.gcpw_pinned_version = GcpwVersion("100.1.2.3");

  // Create a device policy by merging the two users policies.
  DevicePolicies device_policy =
      DevicePolicies::FromUserPolicies(existing_user_policy);
  device_policy.MergeWith(DevicePolicies::FromUserPolicies(new_user_policy));

  // The new policy should allow everything the existing user was able to do
  // before.
  ASSERT_EQ(existing_user_policy.enable_dm_enrollment,
            device_policy.enable_dm_enrollment);
  ASSERT_EQ(existing_user_policy.enable_gcpw_auto_update,
            device_policy.enable_gcpw_auto_update);
  ASSERT_EQ(existing_user_policy.enable_multi_user_login,
            device_policy.enable_multi_user_login);

  // The GCPW version should be the latest allowed.
  GcpwVersion gcpw_version = std::max(existing_user_policy.gcpw_pinned_version,
                                      new_user_policy.gcpw_pinned_version);
  ASSERT_EQ(gcpw_version, device_policy.gcpw_pinned_version);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpDevicePoliciesMergeTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Values("99.1.2.3",
                                                              "100.1.2.3",
                                                              "100.1.2.4")));

}  // namespace testing
}  // namespace credential_provider
