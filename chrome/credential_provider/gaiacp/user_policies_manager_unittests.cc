// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/base_paths_win.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/user_policies_manager.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class GcpUserPoliciesBaseTest : public GlsRunnerTestBase {};

TEST_F(GcpUserPoliciesBaseTest, NonExistentUser) {
  ASSERT_TRUE(FAILED(UserPoliciesManager::Get()->FetchAndStoreCloudUserPolicies(
      L"not-valid-sid", "not-valid-token")));
  UserPolicies policies;
  ASSERT_FALSE(
      UserPoliciesManager::Get()->GetUserPolicies(L"not-valid", &policies));
}

class GcpUserPoliciesFetchAndReadTest
    : public GcpUserPoliciesBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, bool, const char*, bool, int>> {
 protected:
  void SetUp() override;
  void SetRegistryValues(bool dm_enrollment,
                         bool multi_user,
                         DWORD validity_days);

  UserPolicies policies_;
  base::string16 sid_;
};

void GcpUserPoliciesFetchAndReadTest::SetUp() {
  GcpUserPoliciesBaseTest::SetUp();

  policies_.enable_dm_enrollment = std::get<0>(GetParam());
  policies_.enable_gcpw_auto_update = std::get<1>(GetParam());
  policies_.gcpw_pinned_version = GcpwVersion(std::get<2>(GetParam()));
  policies_.enable_multi_user_login = std::get<3>(GetParam());
  policies_.validity_period_days = std::get<4>(GetParam());

  // Create a fake user associated to a gaia id.
  CComBSTR sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                kDefaultUsername, L"password", L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), L"user@company.com", &sid));
  sid_ = OLE2W(sid);
}

void GcpUserPoliciesFetchAndReadTest::SetRegistryValues(bool dm_enrollment,
                                                        bool multi_user,
                                                        DWORD validity_days) {
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment,
                                          dm_enrollment ? 1 : 0));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser,
                                          multi_user ? 1 : 0));
  ASSERT_EQ(S_OK,
            SetGlobalFlagForTesting(base::UTF8ToUTF16(kKeyValidityPeriodInDays),
                                    validity_days));
}

TEST_P(GcpUserPoliciesFetchAndReadTest, ValueConversion) {
  base::Value policies_value = policies_.ToValue();
  UserPolicies policies_from_value = UserPolicies::FromValue(policies_value);

  ASSERT_EQ(policies_, policies_from_value);
}

TEST_P(GcpUserPoliciesFetchAndReadTest, CloudPoliciesWin) {
  // Set conflicting policy values in registry.
  SetRegistryValues(!policies_.enable_dm_enrollment,
                    !policies_.enable_multi_user_login,
                    policies_.validity_period_days + 100);

  base::Value policies_value = policies_.ToValue();
  std::string expected_response;
  base::JSONWriter::Write(policies_value, &expected_response);

  GURL user_policies_url =
      UserPoliciesManager::Get()->GetGcpwServiceUserPoliciesUrl(sid_);

  ASSERT_TRUE(user_policies_url.is_valid());
  ASSERT_NE(std::string::npos, user_policies_url.spec().find(kDefaultGaiaId));

  // Set valid cloud policies for all settings.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      user_policies_url, FakeWinHttpUrlFetcher::Headers(), expected_response);

  ASSERT_TRUE(
      SUCCEEDED(UserPoliciesManager::Get()->FetchAndStoreCloudUserPolicies(
          sid_, "access_token")));

  UserPolicies policies_fetched;
  ASSERT_TRUE(
      UserPoliciesManager::Get()->GetUserPolicies(sid_, &policies_fetched));

  ASSERT_EQ(policies_, policies_fetched);
}

TEST_P(GcpUserPoliciesFetchAndReadTest, RegistryValuesWin) {
  // Set expected values in registry.
  SetRegistryValues(policies_.enable_dm_enrollment,
                    policies_.enable_multi_user_login,
                    policies_.validity_period_days);

  // Only set values for cloud policies for those not already set in registry.
  base::Value policies_value(base::Value::Type::DICTIONARY);
  policies_value.SetBoolKey("enable_gcpw_auto_update",
                            policies_.enable_gcpw_auto_update);
  policies_value.SetStringKey("gcpw_pinned_version",
                              policies_.gcpw_pinned_version.ToString());
  std::string expected_response;
  base::JSONWriter::Write(policies_value, &expected_response);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      UserPoliciesManager::Get()->GetGcpwServiceUserPoliciesUrl(sid_),
      FakeWinHttpUrlFetcher::Headers(), expected_response);

  ASSERT_TRUE(
      SUCCEEDED(UserPoliciesManager::Get()->FetchAndStoreCloudUserPolicies(
          sid_, "access_token")));

  UserPolicies policies_fetched;
  // Also check if the defaults conform to the registry values.
  ASSERT_EQ(policies_.enable_dm_enrollment,
            policies_fetched.enable_dm_enrollment);
  ASSERT_EQ(policies_.enable_multi_user_login,
            policies_fetched.enable_multi_user_login);
  ASSERT_EQ(policies_.validity_period_days,
            policies_fetched.validity_period_days);

  ASSERT_TRUE(
      UserPoliciesManager::Get()->GetUserPolicies(sid_, &policies_fetched));

  ASSERT_EQ(policies_, policies_fetched);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpUserPoliciesFetchAndReadTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Values("", "110.2.33.2"),
                                            ::testing::Bool(),
                                            ::testing::Values(0, 30)));

}  // namespace testing
}  // namespace credential_provider
