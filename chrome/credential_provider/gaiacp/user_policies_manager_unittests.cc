// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/user_policies_manager.h"

#include <windows.h>

#include "base/base_paths_win.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "chrome/credential_provider/extension/user_device_context.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class GcpUserPoliciesBaseTest : public GlsRunnerTestBase {
 protected:
  void SetUp() override;
  std::wstring CreateUser();
};

void GcpUserPoliciesBaseTest::SetUp() {
  GlsRunnerTestBase::SetUp();

  FakesForTesting fakes;
  fakes.fake_win_http_url_fetcher_creator =
      fake_http_url_fetcher_factory()->GetCreatorCallback();
  fakes.internet_availability_checker_for_testing = fake_internet_checker();
  UserPoliciesManager::Get()->SetFakesForTesting(&fakes);
}

std::wstring GcpUserPoliciesBaseTest::CreateUser() {
  // Create a fake user associated to a gaia id.
  CComBSTR sid_str;
  EXPECT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      kDefaultUsername, L"password", L"Full Name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), L"user@company.com",
                      &sid_str));
  return OLE2W(sid_str);
}

TEST_F(GcpUserPoliciesBaseTest, NonExistentUser) {
  ASSERT_TRUE(FAILED(UserPoliciesManager::Get()->FetchAndStoreCloudUserPolicies(
      L"not-valid-sid", "not-valid-token")));
  UserPolicies policies;
  ASSERT_FALSE(
      UserPoliciesManager::Get()->GetUserPolicies(L"not-valid", &policies));
}

TEST_F(GcpUserPoliciesBaseTest, NoAccessToken) {
  std::wstring sid = CreateUser();

  ASSERT_TRUE(FAILED(
      UserPoliciesManager::Get()->FetchAndStoreCloudUserPolicies(sid, "")));
  UserPolicies policies;
  ASSERT_FALSE(UserPoliciesManager::Get()->GetUserPolicies(sid, &policies));
}

TEST_F(GcpUserPoliciesBaseTest, DetectMissingAndStalePolicies) {
  fake_internet_checker()->SetHasInternetConnection(
      FakeInternetAvailabilityChecker::kHicForceYes);

  std::wstring sid = CreateUser();
  ASSERT_TRUE(UserPoliciesManager::Get()->IsUserPolicyStaleOrMissing(sid));

  UserPolicies policies;
  base::Value::Dict expected_response_value =
      base::Value::Dict().Set("policies", policies.ToValue());
  std::string expected_response;
  base::JSONWriter::Write(expected_response_value, &expected_response);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      UserPoliciesManager::Get()->GetGcpwServiceUserPoliciesUrl(sid),
      FakeWinHttpUrlFetcher::Headers(), expected_response);

  ASSERT_TRUE(
      SUCCEEDED(UserPoliciesManager::Get()->FetchAndStoreCloudUserPolicies(
          sid, "access_token")));

  ASSERT_FALSE(UserPoliciesManager::Get()->IsUserPolicyStaleOrMissing(sid));

  std::wstring fetch_time_millis = base::NumberToWString(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds() -
      kMaxTimeDeltaSinceLastUserPolicyRefresh.InMilliseconds() - 1);

  ASSERT_EQ(S_OK, SetUserProperty(sid, L"last_policy_refresh_time",
                                  fetch_time_millis));
  ASSERT_TRUE(UserPoliciesManager::Get()->IsUserPolicyStaleOrMissing(sid));

  // When the internet connection doesn't exist, this will return false to avoid
  // online reauth scenarios.
  fake_internet_checker()->SetHasInternetConnection(
      FakeInternetAvailabilityChecker::kHicForceNo);
  ASSERT_TRUE(!UserPoliciesManager::Get()->IsUserPolicyStaleOrMissing(sid));
  fake_internet_checker()->SetHasInternetConnection(
      FakeInternetAvailabilityChecker::kHicForceYes);
}

// Tests effective user policy under various scenarios of cloud policy values.
// Params:
// bool : Whether device management enabled.
// bool : Whether automatic updated enabled.
// string : Pinned version of GCPW to use if any.
// bool : Whether multiple users can login.
// int : Number of days user can remain offline.
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
  std::wstring sid_;
};

void GcpUserPoliciesFetchAndReadTest::SetUp() {
  GcpUserPoliciesBaseTest::SetUp();

  policies_.enable_dm_enrollment = std::get<0>(GetParam());
  policies_.enable_gcpw_auto_update = std::get<1>(GetParam());
  policies_.gcpw_pinned_version = GcpwVersion(std::get<2>(GetParam()));
  policies_.enable_multi_user_login = std::get<3>(GetParam());
  policies_.validity_period_days = std::get<4>(GetParam());

  sid_ = CreateUser();

  // Remove the mdm_url value which exists by default as it's added in
  // InitializeRegistryOverrideForTesting and set to an empty value disabling
  // MDM enrollment.
  base::win::RegKey key;
  EXPECT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_WRITE));
  EXPECT_EQ(ERROR_SUCCESS, key.DeleteValue(kRegMdmUrl));
}

void GcpUserPoliciesFetchAndReadTest::SetRegistryValues(bool dm_enrollment,
                                                        bool multi_user,
                                                        DWORD validity_days) {
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment,
                                          dm_enrollment ? 1 : 0));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser,
                                          multi_user ? 1 : 0));
  ASSERT_EQ(S_OK,
            SetGlobalFlagForTesting(base::UTF8ToWide(kKeyValidityPeriodInDays),
                                    validity_days));
}

TEST_P(GcpUserPoliciesFetchAndReadTest, ValueConversion) {
  base::Value policies_value = policies_.ToValue();
  UserPolicies policies_from_value =
      UserPolicies::FromValue(policies_value.GetDict());

  ASSERT_EQ(policies_, policies_from_value);
}

TEST_P(GcpUserPoliciesFetchAndReadTest, CloudPoliciesWin) {
  // Set conflicting policy values in registry.
  SetRegistryValues(!policies_.enable_dm_enrollment,
                    !policies_.enable_multi_user_login,
                    policies_.validity_period_days + 100);

  base::Value::Dict expected_response_value =
      base::Value::Dict().Set("policies", policies_.ToValue());
  std::string expected_response;
  base::JSONWriter::Write(expected_response_value, &expected_response);

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
  ASSERT_FALSE(UserPoliciesManager::Get()->IsUserPolicyStaleOrMissing(sid_));

  ASSERT_EQ(policies_, policies_fetched);
}

TEST_P(GcpUserPoliciesFetchAndReadTest, RegistryValuesWin) {
  // Set expected values in registry.
  SetRegistryValues(policies_.enable_dm_enrollment,
                    policies_.enable_multi_user_login,
                    policies_.validity_period_days);

  // Only set values for cloud policies for those not already set in registry.
  base::Value::Dict expected_response_value = base::Value::Dict().Set(
      "policies",
      base::Value::Dict()
          .Set("enableGcpwAutoUpdate", policies_.enable_gcpw_auto_update)
          .Set("gcpwPinnedVersion", policies_.gcpw_pinned_version.ToString()));
  std::string expected_response;
  base::JSONWriter::Write(expected_response_value, &expected_response);

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

// Tests user policy fetch by ESA service.
// Params:
// string : The specified device resource ID.
// bool : Whether a valid user sid is present.
// string : The specified DM token.
class GcpUserPoliciesExtensionTest
    : public GcpUserPoliciesBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<const wchar_t*, bool, const wchar_t*>> {
 public:
  GcpUserPoliciesExtensionTest();

 protected:
  extension::TaskCreator fetch_policy_task_creator_;
};

GcpUserPoliciesExtensionTest::GcpUserPoliciesExtensionTest() {
  fetch_policy_task_creator_ =
      UserPoliciesManager::GetFetchPoliciesTaskCreator();
}

TEST_P(GcpUserPoliciesExtensionTest, WithUserDeviceContext) {
  const std::wstring device_resource_id(std::get<0>(GetParam()));
  bool has_valid_sid = std::get<1>(GetParam());
  const std::wstring dm_token(std::get<2>(GetParam()));

  const bool request_can_succeed =
      has_valid_sid && !device_resource_id.empty() && !dm_token.empty();

  std::wstring user_sid = L"invalid-user-sid";
  if (has_valid_sid) {
    // Create a fake user associated to a gaia id.
    CComBSTR sid_str;
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        kDefaultUsername, L"password", L"Full Name", L"comment",
                        base::UTF8ToWide(kDefaultGaiaId), L"user@company.com",
                        &sid_str));
    user_sid = OLE2W(sid_str);
  }

  UserPolicies policies;
  policies.gcpw_pinned_version = GcpwVersion("1.2.3.4");
  base::Value::Dict expected_response_value =
      base::Value::Dict().Set("policies", policies.ToValue());
  std::string expected_response;
  base::JSONWriter::Write(expected_response_value, &expected_response);

  GURL user_policies_url =
      UserPoliciesManager::Get()->GetGcpwServiceUserPoliciesUrl(
          user_sid, device_resource_id, dm_token);

  if (request_can_succeed) {
    ASSERT_TRUE(user_policies_url.is_valid());
    ASSERT_NE(std::string::npos, user_policies_url.spec().find(kDefaultGaiaId));
    ASSERT_NE(std::string::npos, user_policies_url.spec().find(
                                     base::WideToUTF8(device_resource_id)));
    ASSERT_NE(std::string::npos,
              user_policies_url.spec().find(base::WideToUTF8(dm_token)));
  } else {
    ASSERT_FALSE(user_policies_url.is_valid());
  }

  // Set cloud policy fetch server response.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      user_policies_url, FakeWinHttpUrlFetcher::Headers(), expected_response);

  extension::UserDeviceContext context(device_resource_id, L"", L"", user_sid,
                                       dm_token);

  auto task(fetch_policy_task_creator_.Run());
  ASSERT_TRUE(task);

  ASSERT_TRUE(SUCCEEDED(task->SetContext({context})));
  HRESULT status = task->Execute();

  UserPolicies fetched_policies;
  if (!has_valid_sid || device_resource_id.empty() || dm_token.empty()) {
    ASSERT_TRUE(FAILED(status));
    ASSERT_FALSE(UserPoliciesManager::Get()->GetUserPolicies(
        user_sid, &fetched_policies));
  } else {
    ASSERT_TRUE(SUCCEEDED(status));
    ASSERT_TRUE(UserPoliciesManager::Get()->GetUserPolicies(user_sid,
                                                            &fetched_policies));
    ASSERT_EQ(policies, fetched_policies);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpUserPoliciesExtensionTest,
    ::testing::Combine(::testing::Values(L"", L"valid-device-resource-id"),
                       ::testing::Bool(),
                       ::testing::Values(L"", L"valid-dm-token")));

// Test to verify cloud policies can be disabled from registry.
// Parameters:
// string : Value of DM Token on the device.
// int : 0 - Cloud policies disabled through registry.
//       1 - Cloud policies enabled through registry.
//       2 - Cloud policies registry flag not set.
class GcpUserPoliciesDisableFromRegistryTest
    : public GcpUserPoliciesBaseTest,
      public ::testing::WithParamInterface<std::tuple<const char*, int>> {};

TEST_P(GcpUserPoliciesDisableFromRegistryTest, DisableIfSet) {
  std::string dm_token(std::get<0>(GetParam()));
  int reg_enable_cloud_policies = std::get<1>(GetParam());

  if (!dm_token.empty()) {
    ASSERT_EQ(S_OK, SetDmTokenForTesting(dm_token));
  }
  if (reg_enable_cloud_policies < 2) {
    SetGlobalFlagForTesting(L"cloud_policies_enabled",
                            reg_enable_cloud_policies);
  }

  // This is needed because we want to call the default constructor of the
  // UserDeviceManager in each test.
  FakeUserPoliciesManager fake_user_policies_manager;

  // Feature is enabled if it's explicitly enabled or if the flag is not set
  // whether a valid DM token exists or not.
  if (reg_enable_cloud_policies == 1 || reg_enable_cloud_policies == 2) {
    ASSERT_TRUE(UserPoliciesManager::Get()->CloudPoliciesEnabled());
  } else {
    ASSERT_FALSE(UserPoliciesManager::Get()->CloudPoliciesEnabled());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpUserPoliciesDisableFromRegistryTest,
                         ::testing::Combine(::testing::Values("", "dm-token"),
                                            ::testing::Values(0, 1, 2)));

}  // namespace testing
}  // namespace credential_provider
