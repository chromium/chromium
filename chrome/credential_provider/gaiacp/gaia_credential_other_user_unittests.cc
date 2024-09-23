// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl/client.h>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/atl.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential.h"
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

class GcpGaiaCredentialOtherUserTest : public GlsRunnerTestBase {
 protected:
  GcpGaiaCredentialOtherUserTest();

  BSTR signin_result() { return signin_result_; }

  CComBSTR MakeSigninResult(const std::string& password);

 private:
  CComBSTR signin_result_;
};

GcpGaiaCredentialOtherUserTest::GcpGaiaCredentialOtherUserTest() {
  signin_result_ = MakeSigninResult("password");
  // Set the other user tile so that we can get the anonymous credential
  // that may try create a new user.
  fake_user_array()->SetAccountOptions(CPAO_EMPTY_LOCAL);
}

CComBSTR GcpGaiaCredentialOtherUserTest::MakeSigninResult(
    const std::string& password) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;
  test_data_storage.SetSigninPassword(password);

  std::string signin_result_utf8;
  EXPECT_TRUE(base::JSONWriter::Write(test_data_storage.expected_full_result(),
                                      &signin_result_utf8));
  return CComBSTR(A2OLE(signin_result_utf8.c_str()));
}

TEST_F(GcpGaiaCredentialOtherUserTest, OnUserAuthenticated) {
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

TEST_F(GcpGaiaCredentialOtherUserTest, OnUserAuthenticated_SamePassword) {
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

TEST_F(GcpGaiaCredentialOtherUserTest, OnUserAuthenticated_DiffPassword) {
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

class GcpOtherUserCredentialGlsRunnerTest : public GlsRunnerTestBase {};

// Tests the GetUserGlsCommandline method overridden by IGaiaCredential.
// Parameters are:
// 1. Is gem features enabled / disabled.
// 2. Is ep_url already set via registry.
class GcpOtherUserCredentialGlsTest
    : public GcpOtherUserCredentialGlsRunnerTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {};

TEST_P(GcpOtherUserCredentialGlsTest, GetUserGlsCommandLine) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  const bool is_gem_features_enabled = std::get<0>(GetParam());
  if (is_gem_features_enabled)
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kKeyEnableGemFeatures, 1u));
  else
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kKeyEnableGemFeatures, 0u));

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

INSTANTIATE_TEST_SUITE_P(All,
                         GcpOtherUserCredentialGlsTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));
}  // namespace testing

}  // namespace credential_provider
