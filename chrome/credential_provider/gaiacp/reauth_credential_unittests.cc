// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/reauth_credential.h"

#include <wrl/client.h>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/atl.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class GcpReauthCredentialTest : public ::testing::Test {
 protected:
  FakeOSUserManager* fake_os_user_manager() { return &fake_os_user_manager_; }

 private:
  void SetUp() override;

  registry_util::RegistryOverrideManager registry_override_;
  FakeInternetAvailabilityChecker fake_internet_checker_;
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
};

void GcpReauthCredentialTest::SetUp() {
  fake_internet_checker_.SetHasInternetConnection(
      FakeInternetAvailabilityChecker::kHicForceYes);
  InitializeRegistryOverrideForTesting(&registry_override_);
}

TEST_F(GcpReauthCredentialTest, SetOSUserInfoAndReauthEmail) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  Microsoft::WRL::ComPtr<IReauthCredential> reauth;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CReauthCredential>>::CreateInstance(
                      nullptr, IID_IReauthCredential, (void**)&reauth));
  ASSERT_TRUE(reauth);

  const CComBSTR kSid(W2COLE(L"sid"));
  ASSERT_EQ(S_OK, reauth->SetOSUserInfo(
                      kSid, CComBSTR(OSUserManager::GetLocalDomain().c_str()),
                      CComBSTR(W2COLE(L"username"))));
  ASSERT_EQ(S_OK, reauth->SetEmailForReauth(CComBSTR(
                      A2COLE(test_data_storage.GetSuccessEmail().c_str()))));

  Microsoft::WRL::ComPtr<ICredentialProviderCredential2> cpc2;
  ASSERT_EQ(S_OK, reauth.As(&cpc2));
  wchar_t* sid;
  CComBSTR username;
  ASSERT_EQ(S_OK, cpc2->GetUserSid(&sid));
  ASSERT_EQ(kSid, CComBSTR(W2COLE(sid)));
  ::CoTaskMemFree(sid);
}

// Tests the GetStringValue method specific to FID_DESCRIPTION label for AD and
// non-AD user scenarios with existence of email & non-existence scenarios.
// Parameters are:
// 1. Is active directory association registry flag enabled / disabled.
// 2. Email registry entry is set or unset.
// 3. Is user domain joined or not.
// 4. Is sid empty.
class GcpReauthCredentialGetStringValueTest
    : public GcpReauthCredentialTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
};

TEST_P(GcpReauthCredentialGetStringValueTest, FidDescription) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  const bool is_ad_association_enabled = std::get<0>(GetParam());
  const bool is_email_entry_set = std::get<1>(GetParam());
  const bool is_user_domain_joined = std::get<2>(GetParam());
  const bool is_sid_empty = std::get<3>(GetParam());

  // Override registry to enable cloud association with google.
  constexpr wchar_t kRegCloudAssociation[] = L"enable_cloud_association";
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegCloudAssociation,
                                          is_ad_association_enabled));

  Microsoft::WRL::ComPtr<IReauthCredential> reauth;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CReauthCredential>>::CreateInstance(
                      nullptr, IID_IReauthCredential, (void**)&reauth));
  ASSERT_TRUE(reauth);

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create a fake user to reauth.
  CComBSTR sid = nullptr;
  if (!is_sid_empty && is_user_domain_joined) {
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                        L"comment",
                        base::UTF8ToWide(test_data_storage.GetSuccessId()),
                        OLE2CW(email), L"domain", &sid));
  } else if (!is_sid_empty) {
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                        L"comment",
                        base::UTF8ToWide(test_data_storage.GetSuccessId()),
                        OLE2CW(email), &sid));
  }

  if (!is_sid_empty) {
    ASSERT_EQ(S_OK, reauth->SetOSUserInfo(sid, CComBSTR(L"domain"),
                                          CComBSTR(W2COLE(L"username"))));
  }

  if (is_email_entry_set) {
    ASSERT_EQ(S_OK, reauth->SetEmailForReauth(CComBSTR(email)));
  }

  Microsoft::WRL::ComPtr<ICredentialProviderCredential2> cpc2;
  ASSERT_EQ(S_OK, reauth.As(&cpc2));
  LPWSTR string_value = nullptr;
  ASSERT_EQ(S_OK, cpc2->GetStringValue(FID_DESCRIPTION, &string_value));

  if (is_sid_empty) {
    ASSERT_TRUE(string_value[0] == 0);
  } else if (!is_email_entry_set && is_ad_association_enabled &&
             is_user_domain_joined) {
    ASSERT_STREQ(
        string_value,
        W2COLE(GetStringResource(IDS_REAUTH_AD_NO_USER_FID_DESCRIPTION_BASE)
                   .c_str()));
  } else {
    ASSERT_STREQ(
        string_value,
        W2COLE(GetStringResource(IDS_REAUTH_FID_DESCRIPTION_BASE).c_str()));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpReauthCredentialGetStringValueTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Tests the GetStringValue method specific to FID_DESCRIPTION label for reasons
// to enforce GLS. Parameters are:
// 1. Is enrolled with mdm.
// 2. Is encrypted data missing in lsa store.
// 3. Is online login stale.
// 4. Is online login enforced.
class GcpReauthCredentialEnforceAuthReasonGetStringValueTest
    : public GcpReauthCredentialTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 protected:
  FakeAssociatedUserValidator* fake_associated_user_validator() {
    return &fake_associated_user_validator_;
  }

 private:
  FakeAssociatedUserValidator fake_associated_user_validator_;
};

// Disabled due to consistent failure due to mismatching messages.
// See also https://crbug.com/1030602.
TEST_P(GcpReauthCredentialEnforceAuthReasonGetStringValueTest,
       DISABLED_FidDescription) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  const bool enrolled_mdm = std::get<0>(GetParam());
  const bool store_encrypted_data = std::get<1>(GetParam());
  const bool is_stale_login = std::get<2>(GetParam());
  const bool is_online_login_enforced = std::get<3>(GetParam());

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegDisablePasswordSync, 0));

  GoogleMdmEnrolledStatusForTesting forced_enrolled_status(enrolled_mdm);

  Microsoft::WRL::ComPtr<IReauthCredential> reauth;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CReauthCredential>>::CreateInstance(
                      nullptr, IID_IReauthCredential, (void**)&reauth));
  ASSERT_TRUE(reauth);

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create a fake user to reauth.
  CComBSTR sid = nullptr;

  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                L"comment", base::UTF8ToWide(test_data_storage.GetSuccessId()),
                OLE2CW(email), &sid));

  if (store_encrypted_data) {
    std::wstring store_key = GetUserPasswordLsaStoreKey(OLE2W(sid));

    auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
    EXPECT_TRUE(SUCCEEDED(
        policy->StorePrivateData(store_key.c_str(), L"encrypted_data")));
    EXPECT_TRUE(policy->PrivateDataExists(store_key.c_str()));
  }

  if (is_stale_login) {
    ASSERT_EQ(S_OK,
              SetUserProperty((BSTR)sid,
                              base::UTF8ToWide(std::string(kKeyLastTokenValid)),
                              L"0"));
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(
                        base::UTF8ToWide(std::string(kKeyValidityPeriodInDays)),
                        (DWORD)0));
  }

  if (is_online_login_enforced)
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(
                        base::UTF8ToWide(std::string("enforce_online_login")),
                        (DWORD)1));

  // Populate the associated users list. The created user's token handle
  // should be valid so that no reauth credential is created.
  fake_associated_user_validator()->StartRefreshingTokenHandleValidity();

  ASSERT_EQ(S_OK, reauth->SetOSUserInfo(
                      sid, CComBSTR(OSUserManager::GetLocalDomain().c_str()),
                      CComBSTR(W2COLE(L"username"))));

  ASSERT_EQ(S_OK, reauth->SetEmailForReauth(CComBSTR(email)));

  Microsoft::WRL::ComPtr<ICredentialProviderCredential2> cpc2;
  ASSERT_EQ(S_OK, reauth.As(&cpc2));
  LPWSTR string_value = nullptr;
  ASSERT_EQ(S_OK, cpc2->GetStringValue(FID_DESCRIPTION, &string_value));

  if (!enrolled_mdm) {
    ASSERT_STREQ(
        string_value,
        W2COLE(GetStringResource(
                   IDS_REAUTH_NOT_ENROLLED_WITH_MDM_FID_DESCRIPTION_BASE)
                   .c_str()));
  } else if (!store_encrypted_data) {
    ASSERT_STREQ(
        string_value,
        W2COLE(
            GetStringResource(
                IDS_REAUTH_MISSING_PASSWORD_RECOVERY_INFO_FID_DESCRIPTION_BASE)
                .c_str()));
  } else if (is_stale_login) {
    ASSERT_STREQ(
        string_value,
        W2COLE(GetStringResource(IDS_REAUTH_FID_DESCRIPTION_BASE).c_str()));
  } else if (is_online_login_enforced) {
    ASSERT_STREQ(
        string_value,
        W2COLE(GetStringResource(
          IDS_REAUTH_ONLINE_LOGIN_ENFORCED_DESCRIPTION_BASE).c_str()));
  } else {
    ASSERT_STREQ(
        string_value,
        W2COLE(GetStringResource(IDS_REAUTH_FID_DESCRIPTION_BASE).c_str()));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpReauthCredentialEnforceAuthReasonGetStringValueTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class GcpReauthCredentialGlsRunnerTest : public GlsRunnerTestBase {};

// Tests the GetUserGlsCommandline method overridden by ReauthCredential.
// Parameters are:
// 1. Is gem features enabled / disabled.
// 2. Is ep_url already set via registry.
// 3. Does reauth email exist.
// 4. Did user already accept TOS.
class GcpReauthCredentialGlsTest
    : public GcpReauthCredentialGlsRunnerTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
};

TEST_P(GcpReauthCredentialGlsTest, GetUserGlsCommandLine) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  const bool is_gem_features_enabled = std::get<0>(GetParam());
  if (is_gem_features_enabled)
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kKeyEnableGemFeatures, 1u));
  else
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kKeyEnableGemFeatures, 0u));

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());

  // Create a fake user to reauth.
  CComBSTR sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                L"comment", base::UTF8ToWide(test_data_storage.GetSuccessId()),
                std::wstring(), &sid));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  ASSERT_TRUE(cred);

  Microsoft::WRL::ComPtr<IReauthCredential> ireauth;
  ASSERT_EQ(S_OK, cred.As(&ireauth));
  const CComBSTR kSid(W2COLE(L"sid"));
  ASSERT_EQ(S_OK, ireauth->SetOSUserInfo(
                      kSid, CComBSTR(OSUserManager::GetLocalDomain().c_str()),
                      CComBSTR(W2COLE(L"username"))));
  bool set_email_for_reauth = std::get<2>(GetParam());
  if (set_email_for_reauth) {
    ASSERT_EQ(S_OK, ireauth->SetEmailForReauth(CComBSTR(
                        A2COLE(test_data_storage.GetSuccessEmail().c_str()))));
  }

  const bool is_tos_accepted = std::get<3>(GetParam());
  if (is_tos_accepted)
    ASSERT_EQ(S_OK, SetUserProperty(OLE2CW(kSid), kKeyAcceptTos, 1));

  // Get user gls command line and extract the kGaiaUrl &
  // kGcpwEndpointPathSwitch switch from it.
  Microsoft::WRL::ComPtr<ITestCredential> test_cred;
  ASSERT_EQ(S_OK, cred.As(&test_cred));
  std::string device_id;
  ASSERT_EQ(S_OK, GenerateDeviceId(&device_id));

  const bool is_ep_url_set = std::get<1>(GetParam());
  if (is_ep_url_set)
    SetGlobalFlagForTesting(L"ep_reauth_url", L"http://login.com");

  GoogleChromePathForTesting google_chrome_path_for_testing(
      base::FilePath(L"chrome.exe"));
  EXPECT_EQ(S_OK, test_cred->UseRealGlsBaseCommandLine(true));
  base::CommandLine command_line = test_cred->GetTestGlsCommandline();

  EXPECT_TRUE(command_line.HasSwitch(kGcpwSigninSwitch));
  EXPECT_TRUE(command_line.HasSwitch(switches::kDisableExtensions));

  std::string gcpw_path =
      command_line.GetSwitchValueASCII(kGcpwEndpointPathSwitch);

  if (is_ep_url_set) {
    ASSERT_EQ("http://login.com/",
              command_line.GetSwitchValueASCII(switches::kGaiaUrl));
    ASSERT_TRUE(gcpw_path.empty());
  } else if (is_gem_features_enabled) {
    if (set_email_for_reauth) {
      ASSERT_EQ(
          gcpw_path,
          base::StringPrintf("embedded/setup/windows?device_id=%s&show_tos=%d",
                             device_id.c_str(), is_tos_accepted ? 0 : 1));
    } else {
      ASSERT_EQ(
          gcpw_path,
          base::StringPrintf("embedded/setup/windows?device_id=%s&show_tos=%d",
                             device_id.c_str(), is_tos_accepted ? 0 : 1));
    }
    ASSERT_TRUE(command_line.GetSwitchValueASCII(switches::kGaiaUrl).empty());
  } else {
    ASSERT_TRUE(command_line.GetSwitchValueASCII(switches::kGaiaUrl).empty());
    ASSERT_TRUE(gcpw_path.empty());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpReauthCredentialGlsTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

TEST_F(GcpReauthCredentialGlsRunnerTest, NoGaiaIdOrEmailAvailable) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());

  // Create a fake user to reauth.
  CComBSTR sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                L"comment", base::UTF8ToWide(test_data_storage.GetSuccessId()),
                std::wstring(), &sid));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  // Change the registry entry for gaia id and email to empty string.
  ASSERT_EQ(S_OK, SetUserProperty(OLE2CW(sid), kUserId, L""));

  // The GetSerialization call that loads the GLS should fail.
  BOOL auto_login;
  EXPECT_EQ(S_OK, cred->SetSelected(&auto_login));

  // Logging on is an async process, so the call to GetSerialization() starts
  // the process, but when it returns it has not completed.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  ASSERT_EQ(CPGSR_NO_CREDENTIAL_FINISHED, cpgsr);
  EXPECT_STREQ(status_text,
               GetStringResource(IDS_FAILED_CREATE_LOGON_STUB_BASE).c_str());
}

TEST_F(GcpReauthCredentialGlsRunnerTest, NoGaiaIdAvailableForADUser) {
  USES_CONVERSION;

  // Override registry to enable cloud association with google.
  constexpr wchar_t kRegCloudAssociation[] = L"enable_cloud_association";
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegCloudAssociation, 1));

  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create a fake ad joined domain user to reauth.
  CComBSTR sid;
  std::string empty_gaia_id;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                      L"comment", base::UTF8ToWide(empty_gaia_id),
                      OLE2CW(email), L"domain", &sid));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  // The GetSerialization call that loads the GLS should succeed.
  BOOL auto_login;
  EXPECT_EQ(S_OK, cred->SetSelected(&auto_login));

  // Logging on is an async process, so the call to GetSerialization() starts
  // the process, but when it returns it has not completed.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  ASSERT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);
}

TEST_F(GcpReauthCredentialGlsRunnerTest, UserGaiaIdMismatch) {
  USES_CONVERSION;

  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  std::string unexpected_gaia_id = "unexpected-gaia-id";

  // Create an signin result with the unexpected gaia id.
  base::Value::Dict unexpected_full_result =
      test_data_storage.expected_full_result().Clone();
  unexpected_full_result.Set(kKeyId, base::Value(unexpected_gaia_id));
  std::string signin_result_utf8;
  EXPECT_TRUE(
      base::JSONWriter::Write(unexpected_full_result, &signin_result_utf8));
  CComBSTR unexpected_signin_result = A2COLE(signin_result_utf8.c_str());

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create two fake users to reauth. One associated with the valid Gaia id
  // and the other associated to the invalid gaia id.
  CComBSTR first_sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                L"comment", base::UTF8ToWide(test_data_storage.GetSuccessId()),
                std::wstring(), &first_sid));

  CComBSTR second_sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo_bar2", L"pwd2", L"name2", L"comment2",
                      base::UTF8ToWide(unexpected_gaia_id), std::wstring(),
                      &second_sid));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  // Force the GLS to return an invalid Gaia Id without reporting the usual
  // kUiecEMailMissmatch exit code when this happens. This will test whether
  // the credential can perform necessary validation in case the GLS ever
  // does not do the validation for us.
  test->SetGaiaIdOverride(unexpected_gaia_id, /*ignore_expected_gaia_id=*/true);

  // The logon should have failed with an error about another user already
  // associated to this Google account.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, IDS_ACCOUNT_IN_USE_BASE));
}

// Tests the normal reauth scenario.
// 1. Is gem features enabled. If enabled, tos should be tested out.
//    Otherwise, ToS shouldn't be set irrespective of the |kAcceptTos|
//    registry entry.
class GcpNormalReauthCredentialUserSidMismatch
    : public GcpReauthCredentialGlsRunnerTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(GcpNormalReauthCredentialUserSidMismatch, ShouldFail) {
  USES_CONVERSION;

  bool is_non_matching_sid_empty = GetParam();

  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  // Override registry to enable cloud association with google.
  constexpr wchar_t kRegCloudAssociation[] = L"enable_cloud_association";
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegCloudAssociation, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(L"enable_verbose_logging", 1));

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());
  CComBSTR domain = L"domain";

  // Create a fake user to reauth.
  CComBSTR sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                L"comment", base::UTF8ToWide(test_data_storage.GetSuccessId()),
                OLE2CW(email), OLE2CW(domain), &sid));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));
  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));

  // Delete the registry entries and create the environment where
  // GCPW is installed on an AD joined machine and GetSerialization
  // is triggered on a reauth credential.
  RemoveAllUserProperties(OLE2CW(sid));

  // The admin sdk users directory get URL.
  std::string get_cd_user_url_ = base::StringPrintf(
      "https://www.googleapis.com/admin/directory/v1/users/"
      "%s?projection=full&viewType=domain_public",
      base::EscapeUrlEncodedData(base::WideToUTF8(OLE2CW(email)), true)
          .c_str());
  GaiaUrls* gaia_urls_ = GaiaUrls::GetInstance();
  // Set token result as a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  std::string admin_sdk_response;
  CComBSTR different_user = L"different_foo_bar";
  if (!is_non_matching_sid_empty) {
    DWORD error;
    ASSERT_EQ(S_OK, fake_os_user_manager()->AddUser(
                        OLE2W(different_user), L"password", L"fullname",
                        L"comment", true, L"domain", &sid, &error));
  }

  // Set valid response from admin sdk.
  admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": {\"AD_accounts\":"
      " \"%ls/%ls\"}}}",
      L"domain", OLE2CW(different_user));

  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Logon process should fail with an internal error.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, IDS_ACCOUNT_IN_USE_BASE));
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpNormalReauthCredentialUserSidMismatch,
                         ::testing::Values(true, false));

// Tests the normal reauth scenario.
// 1. Is gem features enabled.
// 2. Is tos already accepted.
class GcpNormalReauthCredentialGlsRunnerTest
    : public GcpReauthCredentialGlsRunnerTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {};

TEST_P(GcpNormalReauthCredentialGlsRunnerTest, WithGemFeatures) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  bool is_gem_features_enabled = std::get<0>(GetParam());
  bool is_tos_already_accepted = std::get<1>(GetParam());

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create a fake user to reauth.
  CComBSTR sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                L"comment", base::UTF8ToWide(test_data_storage.GetSuccessId()),
                OLE2CW(email), &sid));

  if (is_gem_features_enabled) {
    // Set |kKeyEnableGemFeatures| registry entry to 1.
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kKeyEnableGemFeatures, 1u));
  } else {
    // Set |kKeyEnableGemFeatures| registry entry to 0.
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kKeyEnableGemFeatures, 0u));
  }

  if (is_tos_already_accepted)
    ASSERT_EQ(S_OK, SetUserProperty(OLE2CW(sid), kKeyAcceptTos, 1u));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Verify command line switch for show_tos.
  if (is_gem_features_enabled && !is_tos_already_accepted)
    ASSERT_EQ("1", test->GetShowTosFromCmdLine());
  else
    ASSERT_EQ("0", test->GetShowTosFromCmdLine());
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpNormalReauthCredentialGlsRunnerTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

TEST_F(GcpReauthCredentialGlsRunnerTest, NormalReauthWithoutEmail) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create a fake user to reauth with no e-mail specified.
  CComBSTR sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                L"comment", base::UTF8ToWide(test_data_storage.GetSuccessId()),
                std::wstring(), &sid));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Email associated should be the default one
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Teardown of the test should confirm that the logon was successful.
}

TEST_F(GcpReauthCredentialGlsRunnerTest, NormalReauthWithoutGaiaId) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());

  // Create a fake user to reauth with no gaia-id specified.
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                      L"comment", std::wstring(),
                      base::UTF8ToWide(kDefaultEmail), &sid));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  // Don't send a forced e-mail. It will be sent from the user that was
  // updated during the last sign in.
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Email associated should be the default one
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Teardown of the test should confirm that the logon was successful.
}

TEST_F(GcpReauthCredentialGlsRunnerTest, GaiaIdMismatch) {
  USES_CONVERSION;
  CredentialProviderSigninDialogTestDataStorage test_data_storage;

  CComBSTR username = L"foo_bar";
  CComBSTR full_name = A2COLE(test_data_storage.GetSuccessFullName().c_str());
  CComBSTR password = A2COLE(test_data_storage.GetSuccessPassword().c_str());
  CComBSTR email = A2COLE(test_data_storage.GetSuccessEmail().c_str());

  // Create a fake user to reauth.
  CComBSTR sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                OLE2CW(username), OLE2CW(password), OLE2CW(full_name),
                L"comment", base::UTF8ToWide(test_data_storage.GetSuccessId()),
                OLE2CW(email), &sid));

  std::string unexpected_gaia_id = "unexpected-gaia-id";

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));
  ASSERT_EQ(S_OK, test->SetGaiaIdOverride(unexpected_gaia_id,
                                          /*ignore_expected_gaia_id=*/false));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // The logon should have failed with an email mismatch error.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, IDS_EMAIL_MISMATCH_BASE));
}

}  // namespace testing

}  // namespace credential_provider
