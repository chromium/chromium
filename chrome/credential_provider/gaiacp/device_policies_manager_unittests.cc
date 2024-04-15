// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/device_policies_manager.h"

#include <windows.h>

#include "base/strings/strcat_win.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
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

  FakesForTesting fakes;
  fakes.fake_win_http_url_fetcher_creator =
      fake_http_url_fetcher_factory()->GetCreatorCallback();
  fakes.os_user_manager_for_testing = fake_os_user_manager();
  UserPoliciesManager::Get()->SetFakesForTesting(&fakes);
}

TEST_F(GcpDevicePoliciesBaseTest, NewUserAssociationWithNoUserPoliciesPresent) {
  FakeUserPoliciesManager fake_user_policies_manager(true);

  // Create a few fake users associated to fake gaia ids.
  std::vector<std::wstring> sids;
  const size_t num_users_needed = 3;
  for (size_t i = 0; i < num_users_needed; ++i) {
    CComBSTR sid_str;
    const std::wstring i_str = base::NumberToWString(i);
    std::wstring username = L"new-user-" + i_str;
    std::wstring gaia_id = L"gaia-id-" + i_str;
    std::wstring email = base::StrCat({L"user_", i_str, L"@company.com"});
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        username, L"password", L"Full Name", L"comment",
                        gaia_id, email, &sid_str));
    sids.push_back(OLE2W(sid_str));
  }

  // Create an existing user association in registry but with an invalid sid.
  base::win::RegKey key;
  std::wstring key_name =
      std::wstring(kGcpUsersRootKeyName) + L"\\non-existent-user-sid";
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

// Base test for testing allowed domains policy scenarios.
class GcpDevicePoliciesAllowedDomainsBaseTest
    : public GcpDevicePoliciesBaseTest {
 public:
  void SetUp() override;
};

void GcpDevicePoliciesAllowedDomainsBaseTest::SetUp() {
  GcpDevicePoliciesBaseTest::SetUp();

  // Delete any existing registry entries. Setting to empty deletes them.
  SetGlobalFlagForTesting(L"ed", L"");
  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"");
}

// Test that correct allowed domains policy is obtained whether they are set in
// the registry or through the cloud policy.
// Parameters are:
// 1. bool : Whether domains set through Omaha cloud policy.
// 2. int  : 0 - Domains not set through registry.
//           1 - Domains set through deprecated "ed" registry entry.
//           2 - Domains set through "domains_allowed_to_login" registry entry.
// 3. string : List of domains from which users are allowed to login.
// 4. bool : Has existing user.
class GcpDevicePoliciesAllowedDomainsTest
    : public GcpDevicePoliciesAllowedDomainsBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, int, const wchar_t*, bool>> {};

TEST_P(GcpDevicePoliciesAllowedDomainsTest, OmahaPolicyTest) {
  bool has_omaha_domains_policy = std::get<0>(GetParam());
  bool has_registry_domains_policy = std::get<1>(GetParam()) != 0;
  bool use_old_domains_reg_key = std::get<1>(GetParam()) == 1;
  std::wstring allowed_domains_str(std::get<2>(GetParam()));
  bool has_existing_user = std::get<3>(GetParam());

  std::vector<std::wstring> allowed_domains = base::SplitString(
      allowed_domains_str, L",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);

  if (has_omaha_domains_policy) {
    ASSERT_TRUE(
        DevicePoliciesManager::Get()->SetAllowedDomainsOmahaPolicyForTesting(
            allowed_domains));
  }

  if (has_registry_domains_policy) {
    if (use_old_domains_reg_key) {
      SetGlobalFlagForTesting(L"ed", allowed_domains_str);
    } else {
      SetGlobalFlagForTesting(L"domains_allowed_to_login", allowed_domains_str);
    }
  }

  FakeUserPoliciesManager fake_user_policies_manager(true);
  UserPolicies user_policy;

  if (has_existing_user) {
    CComBSTR sid;
    ASSERT_EQ(S_OK,
              fake_os_user_manager()->CreateTestOSUser(
                  kDefaultUsername, L"password", L"Full Name", L"comment",
                  base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));
    // Add a random user policy.
    user_policy.enable_dm_enrollment = false;
    user_policy.enable_gcpw_auto_update = false;
    user_policy.enable_multi_user_login = false;
    user_policy.gcpw_pinned_version = GcpwVersion("100.1.2.3");

    fake_user_policies_manager.SetUserPolicies(OLE2W(sid), user_policy);
  }

  DevicePolicies device_policies;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policies);

  if (has_omaha_domains_policy || has_registry_domains_policy)
    ASSERT_EQ(allowed_domains, device_policies.domains_allowed_to_login);

  if (has_existing_user) {
    ASSERT_EQ(user_policy.enable_dm_enrollment,
              device_policies.enable_dm_enrollment);
    ASSERT_EQ(user_policy.enable_gcpw_auto_update,
              device_policies.enable_gcpw_auto_update);
    ASSERT_EQ(user_policy.enable_multi_user_login,
              device_policies.enable_multi_user_login);
    ASSERT_EQ(user_policy.gcpw_pinned_version,
              device_policies.gcpw_pinned_version);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpDevicePoliciesAllowedDomainsTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(0, 1, 2),
        ::testing::Values(L"", L"acme.com", L"acme.com,acme.org"),
        ::testing::Bool()));

// Test to ensure Omaha policies override the existing registry settings for
// allowed domains policy.
// Parameters are:
// 1. bool : If true, deprecated "ed" registry entry is used. Otherwise
//           "domains_allowed_to_login" is used.
// 2. string : List of allowed domains for GCPW specified through registry.
// 3. string : List of allowed domains for GCPW specified through a Omaha cloud
//             policy.
class GcpDevicePoliciesOmahaDomainsWinTest
    : public GcpDevicePoliciesAllowedDomainsBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, const wchar_t*, const wchar_t*>> {};

TEST_P(GcpDevicePoliciesOmahaDomainsWinTest, TestConflict) {
  bool use_old_domains_reg_key = std::get<0>(GetParam());
  std::wstring domains_registry_str(std::get<1>(GetParam()));
  std::wstring domains_from_omaha_str(std::get<2>(GetParam()));

  std::vector<std::wstring> allowed_domains_registry = base::SplitString(
      domains_registry_str, L",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  std::vector<std::wstring> allowed_domains_omaha = base::SplitString(
      domains_from_omaha_str, L",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);

  ASSERT_TRUE(
      DevicePoliciesManager::Get()->SetAllowedDomainsOmahaPolicyForTesting(
          allowed_domains_omaha));

  if (use_old_domains_reg_key) {
    SetGlobalFlagForTesting(L"ed", domains_registry_str);
  } else {
    SetGlobalFlagForTesting(L"domains_allowed_to_login", domains_registry_str);
  }

  DevicePolicies device_policies;
  DevicePoliciesManager::Get()->GetDevicePolicies(&device_policies);

  if (!allowed_domains_omaha.empty()) {
    ASSERT_EQ(allowed_domains_omaha, device_policies.domains_allowed_to_login);
  } else {
    ASSERT_EQ(allowed_domains_registry,
              device_policies.domains_allowed_to_login);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpDevicePoliciesOmahaDomainsWinTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(L"", L"acme.com", L"acme.com,acme.org"),
        ::testing::Values(L"", L"company.com", L"company.com,company.org")));

}  // namespace testing
}  // namespace credential_provider
