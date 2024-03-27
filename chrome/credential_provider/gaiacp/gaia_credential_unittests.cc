// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential.h"

#include <wrl/client.h>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/atl.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class GcpGaiaCredentialTest : public GlsRunnerTestBase {
 protected:
  GcpGaiaCredentialTest();

  BSTR signin_result() { return signin_result_; }

  CComBSTR MakeSigninResult(const std::string& password);

 private:
  CComBSTR signin_result_;
};

GcpGaiaCredentialTest::GcpGaiaCredentialTest() {
  signin_result_ = MakeSigninResult("password");
}

CComBSTR GcpGaiaCredentialTest::MakeSigninResult(const std::string& password) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;
  test_data_storage.SetSigninPassword(password);

  std::string signin_result_utf8;
  EXPECT_TRUE(base::JSONWriter::Write(test_data_storage.expected_full_result(),
                                      &signin_result_utf8));
  return CComBSTR(A2OLE(signin_result_utf8.c_str()));
}

TEST_F(GcpGaiaCredentialTest, OnUserAuthenticated) {
  USES_CONVERSION;

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<IGaiaCredential> gaia_cred;
  ASSERT_EQ(S_OK, cred.As(&gaia_cred));

  CComBSTR error;
  ASSERT_EQ(S_OK, gaia_cred->OnUserAuthenticated(signin_result(), &error));

  Microsoft::WRL::ComPtr<ITestCredentialProvider> test_provider;
  ASSERT_EQ(S_OK, created_provider().As(&test_provider));
  EXPECT_TRUE(test_provider->credentials_changed_fired());
}

TEST_F(GcpGaiaCredentialTest, OnUserAuthenticated_SamePassword) {
  USES_CONVERSION;

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<IGaiaCredential> gaia_cred;
  ASSERT_EQ(S_OK, cred.As(&gaia_cred));

  CComBSTR error;
  ASSERT_EQ(S_OK, gaia_cred->OnUserAuthenticated(signin_result(), &error));

  Microsoft::WRL::ComPtr<ITestCredentialProvider> test_provider;
  ASSERT_EQ(S_OK, created_provider().As(&test_provider));
  CComBSTR first_sid = test_provider->sid();

  // Report to register the user.
  wchar_t* report_status_text = nullptr;
  CREDENTIAL_PROVIDER_STATUS_ICON report_icon;
  EXPECT_EQ(S_OK, cred->ReportResult(0, 0, &report_status_text, &report_icon));

  // Finishing with the same username+password should succeed.
  CComBSTR error2;
  ASSERT_EQ(S_OK, gaia_cred->OnUserAuthenticated(signin_result(), &error2));

  EXPECT_TRUE(test_provider->credentials_changed_fired());
  EXPECT_EQ(first_sid, test_provider->sid());
}

TEST_F(GcpGaiaCredentialTest, OnUserAuthenticated_DiffPassword) {
  USES_CONVERSION;

  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  CComBSTR sid;
  ASSERT_EQ(
      S_OK,
      fake_os_user_manager()->CreateTestOSUser(
          L"foo_bar",
          base::UTF8ToWide(test_data_storage.GetSuccessPassword()).c_str(),
          base::UTF8ToWide(test_data_storage.GetSuccessFullName()).c_str(),
          L"comment",
          base::UTF8ToWide(test_data_storage.GetSuccessId()).c_str(),
          base::UTF8ToWide(test_data_storage.GetSuccessEmail()).c_str(), &sid));
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<IGaiaCredential> gaia_cred;
  ASSERT_EQ(S_OK, cred.As(&gaia_cred));

  CComBSTR error;
  ASSERT_EQ(S_OK, gaia_cred->OnUserAuthenticated(signin_result(), &error));

  Microsoft::WRL::ComPtr<ITestCredentialProvider> test_provider;
  ASSERT_EQ(S_OK, created_provider().As(&test_provider));
  EXPECT_TRUE(test_provider->credentials_changed_fired());

  test_provider->ResetCredentialsChangedFired();

  CComBSTR new_signin_result = MakeSigninResult("password2");

  // Finishing with the same username but different password should mark
  // the password as stale and not fire the credentials changed event.
  EXPECT_EQ(S_FALSE, gaia_cred->OnUserAuthenticated(new_signin_result, &error));
  EXPECT_FALSE(test_provider->credentials_changed_fired());
}

class GcpGaiaCredentialGlsRunnerTest : public GlsRunnerTestBase {};

// Tests the GetUserGlsCommandline method overridden by IGaiaCredential.
// Parameters are:
// 1. Is gem features enabled / disabled.
// 2. Is ep_url already set via registry.
// 3. List of allowed domains.
class GcpGaiaCredentialGlsTest : public GcpGaiaCredentialGlsRunnerTest,
                                 public ::testing::WithParamInterface<
                                     std::tuple<bool, bool, std::wstring>> {};

TEST_P(GcpGaiaCredentialGlsTest, GetUserGlsCommandLine) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  const bool is_gem_features_enabled = std::get<0>(GetParam());
  if (is_gem_features_enabled)
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kKeyEnableGemFeatures, 1u));
  else
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kKeyEnableGemFeatures, 0u));
  const std::wstring email_domains = std::get<2>(GetParam());
  SetGlobalFlagForTesting(L"domains_allowed_to_login", email_domains);

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_TRUE(cred);

  // Get user gls command line and extract the kGaiaUrl &
  // kGcpwEndpointPathSwitch switch from it.
  Microsoft::WRL::ComPtr<ITestCredential> test_cred;
  ASSERT_EQ(S_OK, cred.As(&test_cred));
  std::string device_id;
  ASSERT_EQ(S_OK, GenerateDeviceId(&device_id));

  const bool is_ep_url_set = std::get<1>(GetParam());
  if (is_ep_url_set)
    SetGlobalFlagForTesting(L"ep_setup_url", L"http://login.com");

  GoogleChromePathForTesting google_chrome_path_for_testing(
      base::FilePath(L"chrome.exe"));
  EXPECT_EQ(S_OK, test_cred->UseRealGlsBaseCommandLine(true));
  base::CommandLine command_line = test_cred->GetTestGlsCommandline();
  std::string gcpw_path =
      command_line.GetSwitchValueASCII(kGcpwEndpointPathSwitch);

  EXPECT_TRUE(command_line.HasSwitch(kGcpwSigninSwitch));
  EXPECT_TRUE(command_line.HasSwitch(switches::kDisableExtensions));
  // If domain list has more than one domain, they shouldn't exist in the
  // command line.
  if (email_domains.find(L",") != std::wstring::npos) {
    EXPECT_EQ(command_line.GetSwitchValueASCII(kEmailDomainsSwitch), "");
  } else {
    EXPECT_EQ(command_line.GetSwitchValueASCII(kEmailDomainsSwitch),
              base::WideToUTF8(email_domains));
  }

  if (is_ep_url_set) {
    ASSERT_EQ("http://login.com/",
              command_line.GetSwitchValueASCII(switches::kGaiaUrl));
    ASSERT_TRUE(gcpw_path.empty());
  } else if (is_gem_features_enabled) {
    ASSERT_EQ(gcpw_path, base::StringPrintf(
                             "embedded/setup/windows?device_id=%s&show_tos=1",
                             device_id.c_str()));
    ASSERT_TRUE(command_line.GetSwitchValueASCII(switches::kGaiaUrl).empty());
  } else {
    ASSERT_TRUE(command_line.GetSwitchValueASCII(switches::kGaiaUrl).empty());
    ASSERT_TRUE(gcpw_path.empty());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpGaiaCredentialGlsTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Bool(),
                       ::testing::Values(L"test.com", L"test1.com,test2.com")));

TEST_F(GcpGaiaCredentialGlsRunnerTest,
       AssociateToExistingAssociatedUser_LongUsername) {
  USES_CONVERSION;

  // Create a fake user that has the same username but a different gaia id
  // as the test gaia id.
  CComBSTR sid;
  std::wstring base_username(L"foo1234567890abcdefg");
  std::wstring base_gaia_id(L"other-gaia-id");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      base_username.c_str(), L"password", L"name", L"comment",
                      base_gaia_id, std::wstring(), &sid));

  ASSERT_EQ(2u, fake_os_user_manager()->GetUserCount());

  // Start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<IGaiaCredential> gaia_cred;
  ASSERT_EQ(S_OK, cred.As(&gaia_cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(base::WideToUTF8(base_username) +
                                           "@gmail.com"));
  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // New username should be truncated at the end and have the last character
  // replaced with a new index
  EXPECT_STREQ((base_username.substr(0, base_username.size() - 1) +
                base::NumberToWString(kInitialDuplicateUsernameIndex))
                   .c_str(),
               test->GetFinalUsername());
  // New user should be created.
  EXPECT_EQ(3u, fake_os_user_manager()->GetUserCount());
}

// This test checks the expected success / failure of user creation when
// GCPW wants to associate a gaia id to a user that may already be associated
// to another gaia id.
// Parameters:
// int: Number of existing users to create before trying to associate the
// new user.
// bool: Whether the final user creation is expected to succeed. For
// bool: whether the created users are associated to a gaia id.
// kMaxAttempts = 10, 0 to 8 users can be created and still have the
// test succeed. If a 9th user is create the test will fail.
class GcpAssociatedUserRunnableGaiaCredentialTest
    : public GcpGaiaCredentialGlsRunnerTest,
      public ::testing::WithParamInterface<std::tuple<int, bool, bool>> {};

TEST_P(GcpAssociatedUserRunnableGaiaCredentialTest,
       AssociateToExistingAssociatedUser) {
  USES_CONVERSION;
  int last_user_index = std::get<0>(GetParam());
  bool should_succeed = std::get<1>(GetParam());
  bool should_associate = std::get<2>(GetParam());

  // Create many fake users that has the same username but a different gaia id
  // as the test gaia id.
  CComBSTR sid;
  std::wstring base_username(L"foo");
  std::wstring base_gaia_id(L"other-gaia-id");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      base_username.c_str(), L"password", L"name", L"comment",
                      should_associate ? base_gaia_id : std::wstring(),
                      std::wstring(), &sid));
  ASSERT_EQ(S_OK, SetUserProperty(OLE2CW(sid), kUserId, base_gaia_id));

  for (int i = 0; i < last_user_index; ++i) {
    std::wstring user_suffix =
        base::NumberToWString(i + kInitialDuplicateUsernameIndex);
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        (base_username + user_suffix).c_str(), L"password",
                        L"name", L"comment",
                        should_associate ? base_gaia_id + user_suffix
                                         : std::wstring(),
                        std::wstring(), &sid));
  }

  ASSERT_EQ(static_cast<size_t>(1 + last_user_index + 1),
            fake_os_user_manager()->GetUserCount());

  // Create provider.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<IGaiaCredential> gaia_cred;
  ASSERT_EQ(S_OK, cred.As(&gaia_cred));
  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  // Start logon.
  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  if (should_succeed) {
    EXPECT_STREQ(
        (base_username + base::NumberToWString(last_user_index +
                                               kInitialDuplicateUsernameIndex))
            .c_str(),
        OLE2CW(test->GetFinalUsername()));
    // New user should be created.
    EXPECT_EQ(static_cast<size_t>(last_user_index + 2 + 1),
              fake_os_user_manager()->GetUserCount());

    ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

  } else {
    // No new user should be created.
    EXPECT_EQ(static_cast<size_t>(last_user_index + 1 + 1),
              fake_os_user_manager()->GetUserCount());
    ASSERT_EQ(S_OK, FinishLogonProcess(false, false, IDS_INTERNAL_ERROR_BASE));
  }
}

// For a max retry of 10, it is possible to create users 'username',
// 'username0' ... 'username8' before failing. At 'username9' the test should
// fail.

INSTANTIATE_TEST_SUITE_P(
    AvailableUsername,
    GcpAssociatedUserRunnableGaiaCredentialTest,
    ::testing::Combine(::testing::Range(0, kMaxUsernameAttempts - 2),
                       ::testing::Values(true),
                       ::testing::Values(true, false)));

INSTANTIATE_TEST_SUITE_P(
    UnavailableUsername,
    GcpAssociatedUserRunnableGaiaCredentialTest,
    ::testing::Combine(::testing::Values(kMaxUsernameAttempts - 1),
                       ::testing::Values(false),
                       ::testing::Values(true, false)));
}  // namespace testing

}  // namespace credential_provider
