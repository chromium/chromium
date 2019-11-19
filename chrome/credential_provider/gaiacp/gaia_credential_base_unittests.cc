// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <sddl.h>  // For ConvertSidToStringSid()
#include <wrl/client.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time_override.h"

#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/password_recovery_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

constexpr char kFakeResourceId[] = "fake_resource_id";

// DER-encoded, PKIX public key.
constexpr char kTestPublicKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAlFxKse4DGwIDQKLN/4Su\n"
    "TvF6+J5Juv/Ywwovws+UV7UmXDCRPaaFj36u9LpIqzja2/KG+17Ob7L4KDLLIe6g\n"
    "mJ2wP9ioawBDJ1JWryNkHcVUcc/bbTgpyD6N0RcpvsbM8YpccYJ1aDAsdKy0593s\n"
    "ozMUBZ9Y7Z3Yb1Xvoq965At6ihD7s0FMNzehCuwrfJ+A47ChIho0IMxpa2NhrQUo\n"
    "1Sjm7NEh5u9xTzH+5VtGLJnF5FJ6fWy2YEUfMUM9TxrPPDt795UQj5MyVjph0Ssp\n"
    "vXuLQ1Ub7zonhhRcfXi/iCC42n+lpW9TeECKXxj/4xAP4Gqq/VoF1Sr1M6+aZTK5\n"
    "qwIDAQAB";

// DER-encoded, PKCS#8 private key.
const char kTestPrivateKey[] =
    "MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQCUXEqx7gMbAgNAos3/hK5O8"
    "Xr4nkm6/9jDCi/Cz5RXtSZcMJE9poWPfq70ukirONrb8ob7Xs5vsvgoMssh7qCYnbA/2KhrAE"
    "MnUlavI2QdxVRxz9ttOCnIPo3RFym+xszxilxxgnVoMCx0rLTn3eyjMxQFn1jtndhvVe+ir3r"
    "kC3qKEPuzQUw3N6EK7Ct8n4DjsKEiGjQgzGlrY2GtBSjVKObs0SHm73FPMf7lW0YsmcXkUnp9"
    "bLZgRR8xQz1PGs88O3v3lRCPkzJWOmHRKym9e4tDVRvvOieGFFx9eL+IILjaf6Wlb1N4QIpfG"
    "P/jEA/gaqr9WgXVKvUzr5plMrmrAgMBAAECggEACK3liMc900So4A0mM/6VG/Uwln7cHV5+Vd"
    "qwtJrkOMVWOyp0NMEbKyvkHFkRi0LGOvvTPb1sIki8D8346EFHj+YZu4J3R9s6EoDUpWZSoxM"
    "6P3ZDhf41I4vVTBgozwpeTvsjMVjKeY/n6eN4qd/nyhxg3XtW/n+ve8PxQvk1HUYfxokJBkjs"
    "5IF/Nka18Ia/nEjaItnix+tdYPH/e074QorvXR+VYH+YKiOEfVCFH98HyLjsd2g7TOwEzQnzh"
    "ECSR7tAa7Q4EsrwmpPfQ9TJy476CY/RcVe5waLRfpj8medkVEDgqmds+KI/qI/TMJL2aCTfax"
    "1g4yBzzf/ADgyBYQKBgQDRbtChbtTM4srMqsIwO/g2kKzv3b/c3fAKW8HbkHdMRAVswJbBMJ/"
    "OrPO6cxLbpy7CtJzH8A7DSZuVH7oyUTI4xVQRT53MF+dmeDyAdwN8pPeS9pb1o2qCXTBKigKD"
    "pFUccq2T3dm9wHLdIwysa5PziOUoRGrgHFoyijcazLN5OQKBgQC1WSklS+fPwpI9fQOj949gj"
    "osTcK/3QeqS2so5xZaSFUPvJtK3PezFGvyF05FM+3VzpS3wfl0Z30msuAMQL7a9tKGykkoUDs"
    "XgmS+Rg4yoqmzk5nWRuE2AenJZs7rtkyLujrv5QYCG5A7TX5rU+c/GquZbmG4lSZ58hbYOxCC"
    "+AwKBgE+u8PQq/g5CT9TVN3MwrfzcyN+uqDw5uQXH6ZdHfQxoaQP6tqEkhfkVttn+xHMMRe9Q"
    "1sH/pS5KSEbRvn88g3Y0Jgs8Fpa7lZBYOPTL02jOP2AMMF2fYnvdRu1lWxWJJdTgEQjMhPb8T"
    "Pe0STMk7zLeqAnNFjjUsMC/871fmv2JAoGAb7mWl9vD3UPKRQeYDpSeSKaJGFj8kCCUHBWfMS"
    "iCM03WpKgOecY08NpHaUuG4R6qpazGOLwhL6dZBIf5mydKNmXqmNF3whO35T97BvM83Uzh+cP"
    "h+vzJArZtbMZGC8fyZXaaaF3qiTBH0gG8qimd0I/Ji/TFJ0PL2HuoRkCey3ECgYAJh9HYMbVe"
    "9+Sxa1/UL+HSC/AgA8ueMNxzFZ4fI8haab16xefDXwdrHm3PSxt0pn1E1kmTQyP2KPuoOLYas"
    "q6BRf4WzsjBrS1kPrlCwZNZkPqz3QnV4oVT3tW6q9kWyY+WKz0s7byT0AiriRrCLcQbYYYog7"
    "OaEw4i7JOShaPsLQ==";

class GcpGaiaCredentialBaseTest : public GlsRunnerTestBase {};

TEST_F(GcpGaiaCredentialBaseTest, Advise) {
  // Create provider with credentials. This should Advise the credential.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  // Release ref count so the credential can be deleted by the call to
  // ReleaseProvider.
  cred.Reset();

  // Release the provider. This should unadvise the credential.
  ASSERT_EQ(S_OK, ReleaseProvider());
}

TEST_F(GcpGaiaCredentialBaseTest, SetSelected) {
  // Create provider and credential only.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  // A credential that has not attempted to sign in a user yet should return
  // false for |auto_login|.
  BOOL auto_login;
  ASSERT_EQ(S_OK, cred->SetSelected(&auto_login));
  ASSERT_FALSE(auto_login);
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_NoInternet) {
  FakeInternetAvailabilityChecker internet_checker(
      FakeInternetAvailabilityChecker::kHicForceNo);

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcess(/*succeeds=*/false, IDS_NO_NETWORK_BASE));
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_GlsLoadingFailed) {
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));
  // Fail loading the gls logon UI.
  test->FailLoadingGaiaLogonStub();

  ASSERT_EQ(S_OK, StartLogonProcess(
                      /*succeeds=*/false, IDS_FAILED_CREATE_LOGON_STUB_BASE));
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Start) {
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Finish) {
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Make sure a "foo" user was created.
  PSID sid;
  EXPECT_EQ(S_OK, fake_os_user_manager()->GetUserSID(
                      OSUserManager::GetLocalDomain().c_str(), kDefaultUsername,
                      &sid));
  ::LocalFree(sid);

  // New user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

// This test emulates the scenario where SetDeselected is triggered by the
// Windows Login UI process after GetSerialization prior to invocation of
// ReportResult. Note: This currently happens only for OtherUser credential
// workflow.
TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_SetDeselectedBeforeReportResult) {
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Make sure a "foo" user was created.
  PSID sid;
  EXPECT_EQ(S_OK, fake_os_user_manager()->GetUserSID(
                      OSUserManager::GetLocalDomain().c_str(), kDefaultUsername,
                      &sid));

  // New user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Finishing logon process should trigger credential changed and trigger
  // GetSerialization.
  ASSERT_EQ(S_OK, FinishLogonProcessWithCred(true, true, 0, cred));

  // Trigger SetDeselected prior to ReportResult is invoked.
  cred->SetDeselected();

  // Verify that the authentication results dictionary is not empty.
  ASSERT_FALSE(test->IsAuthenticationResultsEmpty());
  ASSERT_FALSE(test->IsAdJoinedUser());

  // Trigger ReportResult and verify that the authentication results are saved
  // into registry and ResetInternalState is triggered.
  ReportLogonProcessResult(cred);

  // Verify that the registry entry for the user was created.
  wchar_t gaia_id[256];
  ULONG length = base::size(gaia_id);
  wchar_t* sidstr = nullptr;
  ::ConvertSidToStringSid(sid, &sidstr);
  ::LocalFree(sid);

  HRESULT gaia_id_hr = GetUserProperty(sidstr, kUserId, gaia_id, &length);
  ASSERT_EQ(S_OK, gaia_id_hr);
  ASSERT_TRUE(gaia_id[0]);

  // Verify that the authentication results dictionary is now empty.
  ASSERT_TRUE(test->IsAuthenticationResultsEmpty());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Abort) {
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));
  ASSERT_EQ(S_OK, test->SetDefaultExitCode(kUiecAbort));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Logon process should not signal credentials change or raise an error
  // message.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_AssociateToMatchingAssociatedUser) {
  USES_CONVERSION;
  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR first_sid;
  base::string16 username(L"foo");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      username, L"password", L"name", L"comment",
                      base::UTF8ToUTF16(kDefaultGaiaId), base::string16(),
                      &first_sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // No new user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_MultipleCalls) {
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  constexpr wchar_t kStartGlsEventName[] =
      L"GetSerialization_MultipleCalls_Wait";
  base::win::ScopedHandle start_event_handle(
      ::CreateEvent(nullptr, false, false, kStartGlsEventName));
  ASSERT_TRUE(start_event_handle.IsValid());
  ASSERT_EQ(S_OK, test->SetStartGlsEventName(kStartGlsEventName));
  base::WaitableEvent start_event(std::move(start_event_handle));

  ASSERT_EQ(S_OK, StartLogonProcess(/*succeeds=*/true));

  // Calling GetSerialization again while the credential is waiting for the
  // logon process should yield CPGSR_NO_CREDENTIAL_NOT_FINISHED as a
  // response.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  EXPECT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPSI_NONE, status_icon);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Signal that the gls process can finish.
  start_event.Signal();

  ASSERT_EQ(S_OK, WaitForLogonProcess());
}

// Test disabling force reset password field. If provided gaia password
// isn't valid, credential goes into password recovery flow. If the force reset
// password is disabled through registry, force reset password field should be
// in CPFS_HIDDEN state.
// 0 - Disable force reset pasword link.
// 1 - Enable force reset password link.
// 2 - Test default value of registry. By default force reset password link
// should be enabled.
class GcpGaiaCredentialBaseForceResetRegistryTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<int> {};

TEST_P(GcpGaiaCredentialBaseForceResetRegistryTest,
       ForceResetPasswordRegistry) {
  int enable_forgot_password_registry_value = GetParam();

  if (enable_forgot_password_registry_value < 2)
    ASSERT_EQ(S_OK,
              SetGlobalFlagForTesting(kRegMdmEnableForcePasswordReset,
                                      enable_forgot_password_registry_value));

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  CComBSTR sid;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(CPFS_HIDDEN,
            fake_credential_provider_credential_events()->GetFieldState(
                cred.Get(), FID_FORGOT_PASSWORD_LINK));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_FALSE(test->IsWindowsPasswordValidForStoredUser());

  if (!enable_forgot_password_registry_value) {
    ASSERT_EQ(CPFS_HIDDEN,
              fake_credential_provider_credential_events()->GetFieldState(
                  cred.Get(), FID_FORGOT_PASSWORD_LINK));
  } else {
    ASSERT_EQ(CPFS_DISPLAY_IN_SELECTED_TILE,
              fake_credential_provider_credential_events()->GetFieldState(
                  cred.Get(), FID_FORGOT_PASSWORD_LINK));
  }

  // Update the Windows password to be the real password created for the user.
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, windows_password);
  // Sign in information should still be available.
  EXPECT_TRUE(test->GetFinalEmail().length());

  // Both Windows and Gaia credentials should be valid now.
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_TRUE(test->IsWindowsPasswordValidForStoredUser());

  // Finish logon successfully but with no credential changed event.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
}

INSTANTIATE_TEST_SUITE_P(,
                         GcpGaiaCredentialBaseForceResetRegistryTest,
                         ::testing::Values(0, 1, 2));

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_PasswordChangedForAssociatedUser) {
  USES_CONVERSION;

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  CComBSTR sid;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(CPFS_HIDDEN,
            fake_credential_provider_credential_events()->GetFieldState(
                cred.Get(), FID_FORGOT_PASSWORD_LINK));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_FALSE(test->IsWindowsPasswordValidForStoredUser());

  ASSERT_EQ(CPFS_DISPLAY_IN_SELECTED_TILE,
            fake_credential_provider_credential_events()->GetFieldState(
                cred.Get(), FID_FORGOT_PASSWORD_LINK));

  // Check that the process has not finished yet.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Credentials should still be available.
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_FALSE(test->IsWindowsPasswordValidForStoredUser());

  // Set an invalid password and try to get serialization again. Credentials
  // should still be valid but serialization is not complete.
  CComBSTR invalid_windows_password = L"a";
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, invalid_windows_password);
  EXPECT_EQ(nullptr, status_text);
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Update the Windows password to be the real password created for the user.
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, windows_password);
  // Sign in information should still be available.
  EXPECT_TRUE(test->GetFinalEmail().length());

  // Both Windows and Gaia credentials should be valid now
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_TRUE(test->IsWindowsPasswordValidForStoredUser());

  // Finish logon successfully but with no credential changed event.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_ForgotPasswordForAssociatedUser) {
  USES_CONVERSION;

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  CComBSTR sid;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_FALSE(test->IsWindowsPasswordValidForStoredUser());

  // Check that the process has not finished yet.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Credentials should still be available.
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_FALSE(test->IsWindowsPasswordValidForStoredUser());

  // Simulate a click on the "Forgot Password" link.
  cred->CommandLinkClicked(FID_FORGOT_PASSWORD_LINK);

  // Finish logon successfully but with no credential changed event.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_AlternateForgotPasswordAssociatedUser) {
  USES_CONVERSION;

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  CComBSTR sid;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_FALSE(test->IsWindowsPasswordValidForStoredUser());

  // Check that the process has not finished yet.
  CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE cpgsr;
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  wchar_t* status_text;
  CREDENTIAL_PROVIDER_STATUS_ICON status_icon;
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(nullptr, status_text);
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Credentials should still be available.
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_FALSE(test->IsWindowsPasswordValidForStoredUser());

  // Simulate a click on the "Forgot Password" link.
  cred->CommandLinkClicked(FID_FORGOT_PASSWORD_LINK);

  // Go back to windows password entry.
  cred->CommandLinkClicked(FID_FORGOT_PASSWORD_LINK);

  // Set an invalid password and try to get serialization again. Credentials
  // should still be valid but serialization is not complete.
  CComBSTR invalid_windows_password = L"a";
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, invalid_windows_password);
  EXPECT_EQ(nullptr, status_text);
  ASSERT_EQ(S_OK,
            cred->GetSerialization(&cpgsr, &cpcs, &status_text, &status_icon));
  EXPECT_EQ(CPGSR_NO_CREDENTIAL_NOT_FINISHED, cpgsr);

  // Update the Windows password to be the real password created for the user.
  cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, windows_password);
  // Sign in information should still be available.
  EXPECT_TRUE(test->GetFinalEmail().length());

  // Both Windows and Gaia credentials should be valid now
  EXPECT_TRUE(test->CanAttemptWindowsLogon());
  EXPECT_TRUE(test->IsWindowsPasswordValidForStoredUser());

  // Finish logon successfully but with no credential changed event.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest, GetSerialization_Cancel) {
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  // This event is merely used to keep the gls running while it is cancelled
  // through SetDeselected().
  constexpr wchar_t kStartGlsEventName[] = L"GetSerialization_Cancel_Signal";
  base::win::ScopedHandle start_event_handle(
      ::CreateEvent(nullptr, false, false, kStartGlsEventName));
  ASSERT_TRUE(start_event_handle.IsValid());
  ASSERT_EQ(S_OK, test->SetStartGlsEventName(kStartGlsEventName));
  base::WaitableEvent start_event(std::move(start_event_handle));

  ASSERT_EQ(S_OK, StartLogonProcess(/*succeeds=*/true));

  // Deselect the credential provider so that it cancels the GLS process and
  // returns.
  ASSERT_EQ(S_OK, cred->SetDeselected());

  ASSERT_EQ(S_OK, WaitForLogonProcess());

  // Logon process should not signal credentials change or raise an error
  // message.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, 0));
}

TEST_F(GcpGaiaCredentialBaseTest, FailedUserCreation) {
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  // Fail user creation.
  fake_os_user_manager()->SetShouldFailUserCreation(true);

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Logon process should fail with an internal error.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, IDS_INTERNAL_ERROR_BASE));
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD) {
  USES_CONVERSION;
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  constexpr char email[] = "foo@imfl.info";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"foo_imfl"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, NewUserDisabledThroughUsageScenario) {
  USES_CONVERSION;
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Set the other user tile so that we can get the anonymous credential
  // that may try create a new user.
  fake_user_array()->SetAccountOptions(CPAO_EMPTY_LOCAL);

  SetUsageScenario(CPUS_UNLOCK_WORKSTATION);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Sign in should fail with an error stating that no new users can be created.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false,
                                     IDS_INVALID_UNLOCK_WORKSTATION_USER_BASE));
}

TEST_F(GcpGaiaCredentialBaseTest, NewUserDisabledThroughMdm) {
  USES_CONVERSION;
  // Enforce single user mode for MDM.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));
  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user that is already associated so when the user tries to
  // sign on and create a new user, it fails.
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo_registered", L"password", L"name", L"comment",
                      L"gaia-id-registered", base::string16(), &sid));

  // Populate the associated users list. The created user's token handle
  // should be valid so that no reauth credential is created.
  fake_associated_user_validator()->StartRefreshingTokenHandleValidity();

  // Set the other user tile so that we can get the anonymous credential
  // that may try to sign in a user.
  fake_user_array()->SetAccountOptions(CPAO_EMPTY_LOCAL);

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Sign in should fail with an error stating that no new users can be created.
  ASSERT_EQ(S_OK,
            FinishLogonProcess(false, false, IDS_ADD_USER_DISALLOWED_BASE));
}

TEST_F(GcpGaiaCredentialBaseTest, InvalidUserUnlockedAfterSignin) {
  // Enforce token handle verification with user locking when the token handle
  // is not valid.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  GoogleMdmEnrollmentStatusForTesting force_success(true);

  USES_CONVERSION;
  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR sid;
  base::string16 username(L"foo");
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                username, L"password", L"name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  // User should have invalid token handle and be locked.
  EXPECT_FALSE(
      fake_associated_user_validator()->IsTokenHandleValidForUser(OLE2W(sid)));
  EXPECT_EQ(true,
            fake_associated_user_validator()->IsUserAccessBlockedForTesting(
                OLE2W(sid)));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Now finish the logon, this should unlock the user.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

  EXPECT_EQ(false,
            fake_associated_user_validator()->IsUserAccessBlockedForTesting(
                OLE2W(sid)));

  // No new user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

TEST_F(GcpGaiaCredentialBaseTest, DenySigninBlockedDuringSignin) {
  USES_CONVERSION;

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR first_sid;
  base::string16 username(L"foo");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      username, L"password", L"name", L"comment",
                      base::UTF8ToUTF16(kDefaultGaiaId), base::string16(),
                      &first_sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with valid token handle response and sign in the anonymous
  // credential with the user that should still be valid.
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Change token response to an invalid one.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(AssociatedUserValidator::kTokenInfoUrl),
      FakeWinHttpUrlFetcher::Headers(), "{}");

  // Force refresh of all token handles on the next query.
  fake_associated_user_validator()->ForceRefreshTokenHandlesForTesting();

  // Signin process has already started. User should not be locked even if their
  // token handle is invalid.
  EXPECT_FALSE(fake_associated_user_validator()
                   ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_FALSE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  // Now finish the logon.
  ASSERT_EQ(S_OK, FinishLogonProcessWithCred(true, true, 0, cred));

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Result has not been reported yet, user signin should still not be denied.
  EXPECT_FALSE(fake_associated_user_validator()
                   ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_FALSE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  ReportLogonProcessResult(cred);

  // Now signin can be denied for the user if their token handle is invalid.
  EXPECT_TRUE(fake_associated_user_validator()
                  ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_TRUE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  // No new user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

class BaseTimeClockOverrideValue {
 public:
  static base::Time NowOverride() { return current_time_; }
  static base::Time current_time_;
};

base::Time BaseTimeClockOverrideValue::current_time_;
TEST_F(GcpGaiaCredentialBaseTest,
       DenySigninBlockedDuringSignin_StaleOnlineLogin) {
  USES_CONVERSION;

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));

  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR first_sid;
  base::string16 username(L"foo");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      username, L"password", L"name", L"comment",
                      base::UTF8ToUTF16(kDefaultGaiaId), base::string16(),
                      &first_sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Move the current time beyond staleness time period.
  base::Time last_online_login = base::Time::Now();
  base::string16 last_online_login_millis = base::NumberToString16(
      last_online_login.ToDeltaSinceWindowsEpoch().InMilliseconds());
  int validity_period_in_days = 10;
  DWORD validity_period_in_days_dword =
      static_cast<DWORD>(validity_period_in_days);
  ASSERT_EQ(S_OK, SetUserProperty((BSTR)first_sid,
                                  base::UTF8ToUTF16(std::string(
                                      kKeyLastSuccessfulOnlineLoginMillis)),
                                  last_online_login_millis));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(
                      base::UTF8ToUTF16(std::string(kKeyValidityPeriodInDays)),
                      validity_period_in_days_dword));

  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with valid token handle response and sign in the anonymous
  // credential with the user that should still be valid.
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  // User access shouldn't be blocked before login starts.
  EXPECT_FALSE(fake_associated_user_validator()
                   ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_FALSE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  // Advance the time that is more than the offline validity period.
  BaseTimeClockOverrideValue::current_time_ =
      last_online_login + base::TimeDelta::FromDays(validity_period_in_days) +
      base::TimeDelta::FromMilliseconds(1);
  base::subtle::ScopedTimeClockOverrides time_override(
      &BaseTimeClockOverrideValue::NowOverride, nullptr, nullptr);

  // User access should be blocked now that the time has been moved.
  ASSERT_TRUE(fake_associated_user_validator()
                  ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_TRUE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Now finish the logon.
  ASSERT_EQ(S_OK, FinishLogonProcessWithCred(true, true, 0, cred));

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  ReportLogonProcessResult(cred);

  // User access shouldn't be blocked after login completes.
  EXPECT_FALSE(fake_associated_user_validator()
                   ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON));
  EXPECT_FALSE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  wchar_t latest_online_login_millis[512];
  ULONG latest_online_login_size = base::size(latest_online_login_millis);
  ASSERT_EQ(S_OK, GetUserProperty(
                      OLE2W(first_sid),
                      base::UTF8ToUTF16(kKeyLastSuccessfulOnlineLoginMillis),
                      latest_online_login_millis, &latest_online_login_size));
  int64_t latest_online_login_millis_int64;
  base::StringToInt64(latest_online_login_millis,
                      &latest_online_login_millis_int64);

  long difference =
      latest_online_login_millis_int64 -
      BaseTimeClockOverrideValue::current_time_.ToDeltaSinceWindowsEpoch()
          .InMilliseconds();
  ASSERT_EQ(0, difference);
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Gmail) {
  USES_CONVERSION;

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  constexpr char email[] = "bar@gmail.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"bar"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Googlemail) {
  USES_CONVERSION;

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  constexpr char email[] = "toto@googlemail.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"toto"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, InvalidUsernameCharacters) {
  USES_CONVERSION;
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  constexpr char email[] = "a\\[]:|<>+=;?*z@gmail.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"a____________z"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailTooLong) {
  USES_CONVERSION;

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  constexpr char email[] = "areallylongemailadressdude@gmail.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"areallylongemailadre"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailTooLong2) {
  USES_CONVERSION;
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  constexpr char email[] = "foo@areallylongdomaindude.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"foo_areallylongdomai"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsNoAt) {
  USES_CONVERSION;
  constexpr char email[] = "foo";

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"foo_gmail"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtCom) {
  USES_CONVERSION;

  constexpr char email[] = "@com";

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"_com"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtDotCom) {
  USES_CONVERSION;

  constexpr char email[] = "@.com";

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"_.com"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

// Test various active directory sign in scenarios.
class GcpGaiaCredentialBaseAdScenariosTest : public GcpGaiaCredentialBaseTest {
 protected:
  void SetUp() override;

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred_;
  // The admin sdk users directory get URL.
  std::string get_cd_user_url_ = base::StringPrintf(
      "https://www.googleapis.com/admin/directory/v1/users/"
      "%s?projection=full&viewType=domain_public",
      net::EscapeUrlEncodedData(kDefaultEmail, true).c_str());
  GaiaUrls* gaia_urls_ = GaiaUrls::GetInstance();
};

void GcpGaiaCredentialBaseAdScenariosTest::SetUp() {
  GcpGaiaCredentialBaseTest::SetUp();

  // Set the device as a domain joined machine.
  fake_os_user_manager()->SetIsDeviceDomainJoined(true);

  // Override registry to enable AD association with google.
  constexpr wchar_t kRegEnableADAssociation[] = L"enable_ad_association";
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableADAssociation, 1));

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred_));
}

// Fetching downscoped access token required for calling admin sdk failed.
// The login attempt would fail in this scenario.
TEST_F(GcpGaiaCredentialBaseAdScenariosTest,
       GetSerialization_WithAD_CallToFetchDownscopedAccessTokenFailed) {
  // Attempt to fetch the token from gaia fails.
  fake_http_url_fetcher_factory()->SetFakeFailedResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()), E_FAIL);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_TRUE(base::size(test->GetFinalEmail()) == 0);

  // Make sure no user was created and the login attempt failed.
  PSID sid = nullptr;
  EXPECT_EQ(
      HRESULT_FROM_WIN32(NERR_UserNotFound),
      fake_os_user_manager()->GetUserSID(
          OSUserManager::GetLocalDomain().c_str(), kDefaultUsername, &sid));
  ASSERT_EQ(nullptr, sid);

  // No new user is created.
  EXPECT_EQ(1ul, fake_os_user_manager()->GetUserCount());

  // TODO(crbug.com/976406): Set the error message appropriately for failure
  // scenarios.
  ASSERT_EQ(S_OK, FinishLogonProcess(
                      /*expected_success=*/false,
                      /*expected_credentials_change_fired=*/false,
                      IDS_INTERNAL_ERROR_BASE));
}

// Empty access token returned.
TEST_F(GcpGaiaCredentialBaseAdScenariosTest,
       GetSerialization_WithAD_EmptyAccessTokenReturned) {
  // Set token result to not contain any access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{}");

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_TRUE(base::size(test->GetFinalEmail()) == 0);

  // Make sure no user was created and the login attempt failed.
  PSID sid = nullptr;
  EXPECT_EQ(
      HRESULT_FROM_WIN32(NERR_UserNotFound),
      fake_os_user_manager()->GetUserSID(
          OSUserManager::GetLocalDomain().c_str(), kDefaultUsername, &sid));
  ASSERT_EQ(nullptr, sid);

  // No new user is created.
  EXPECT_EQ(1ul, fake_os_user_manager()->GetUserCount());

  ASSERT_EQ(S_OK, FinishLogonProcess(
                      /*expected_success=*/false,
                      /*expected_credentials_change_fired=*/false,
                      IDS_EMPTY_ACCESS_TOKEN_BASE));
}

// Empty AD UPN entry is returned via admin sdk.
TEST_F(GcpGaiaCredentialBaseAdScenariosTest,
       GetSerialization_WithAD_NoAdUpnFoundFromAdminSdk) {
  // Set token result a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  // Set empty response from admin sdk.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(), "{}");

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Make sure a "foo" user was created.
  PSID sid;
  EXPECT_EQ(S_OK, fake_os_user_manager()->GetUserSID(
                      OSUserManager::GetLocalDomain().c_str(), kDefaultUsername,
                      &sid));
  ::LocalFree(sid);

  // New user should be created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
}

// Call to the admin sdk to fetch the AD UPN failed.
TEST_F(GcpGaiaCredentialBaseAdScenariosTest,
       GetSerialization_WithAD_CallToAdminSdkFailed) {
  // Set token result a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  // Fail the call from admin sdk.
  fake_http_url_fetcher_factory()->SetFakeFailedResponse(
      GURL(get_cd_user_url_.c_str()), E_FAIL);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_TRUE(base::size(test->GetFinalEmail()) == 0);

  // Make sure no user was created and the login attempt failed.
  PSID sid = nullptr;
  EXPECT_EQ(
      HRESULT_FROM_WIN32(NERR_UserNotFound),
      fake_os_user_manager()->GetUserSID(
          OSUserManager::GetLocalDomain().c_str(), kDefaultUsername, &sid));
  ASSERT_EQ(nullptr, sid);

  // No new user is created.
  EXPECT_EQ(1ul, fake_os_user_manager()->GetUserCount());

  ASSERT_EQ(S_OK, FinishLogonProcess(
                      /*expected_success=*/false,
                      /*expected_credentials_change_fired=*/false,
                      IDS_INTERNAL_ERROR_BASE));
}

// Customer configured invalid ad upn.
TEST_F(GcpGaiaCredentialBaseAdScenariosTest,
       GetSerialization_WithAD_InvalidADUPNConfigured) {
  // Add the user as a domain joined user.
  const wchar_t user_name[] = L"ad_user";
  const wchar_t domain_name[] = L"ad_domain";
  const wchar_t password[] = L"password";

  CComBSTR ad_sid;
  DWORD error;
  HRESULT add_domain_user_hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, domain_name, &ad_sid,
      &error);
  ASSERT_EQ(S_OK, add_domain_user_hr);
  ASSERT_EQ(0u, error);

  // Set token result a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  // Invalid configuration in admin sdk. Don't set the username.
  std::string admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"employeeData\": {\"ad_upn\":"
      " \"%ls/\"}}}",
      domain_name);
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_TRUE(base::size(test->GetFinalEmail()) == 0);

  // Make sure no user was created and the login attempt failed.
  PSID sid = nullptr;
  EXPECT_EQ(
      HRESULT_FROM_WIN32(NERR_UserNotFound),
      fake_os_user_manager()->GetUserSID(
          OSUserManager::GetLocalDomain().c_str(), kDefaultUsername, &sid));
  ASSERT_EQ(nullptr, sid);

  // No new user is created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  ASSERT_EQ(S_OK, FinishLogonProcess(
                      /*expected_success=*/false,
                      /*expected_credentials_change_fired=*/false,
                      IDS_INVALID_AD_UPN_BASE));
}

// This is the success scenario where all preconditions are met in the
// AD login scenario. The user is successfully logged in.
TEST_F(GcpGaiaCredentialBaseAdScenariosTest,
       GetSerialization_WithADSuccessScenario) {
  // Add the user as a domain joined user.
  const wchar_t user_name[] = L"ad_user";
  const wchar_t domain_name[] = L"ad_domain";
  const wchar_t password[] = L"password";

  CComBSTR ad_sid;
  DWORD error;
  HRESULT add_domain_user_hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, domain_name, &ad_sid,
      &error);
  ASSERT_EQ(S_OK, add_domain_user_hr);
  ASSERT_EQ(0u, error);

  // Set token result as a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  // Set valid response from admin sdk.
  std::string admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"employeeData\": {\"ad_upn\":"
      " \"%ls/%ls\"}}}",
      domain_name, user_name);
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);
  ASSERT_TRUE(test->IsAdJoinedUser());

  // Make sure no user was created and the login happens on the
  // existing user instead.
  PSID sid = nullptr;
  EXPECT_EQ(
      HRESULT_FROM_WIN32(NERR_UserNotFound),
      fake_os_user_manager()->GetUserSID(
          OSUserManager::GetLocalDomain().c_str(), kDefaultUsername, &sid));
  ASSERT_EQ(nullptr, sid);

  // Finishing logon process should trigger credential changed and trigger
  // GetSerialization.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

  // Verify that the registry entry for the user was created.
  wchar_t gaia_id[256];
  ULONG length = base::size(gaia_id);
  std::wstring sid_str(ad_sid, SysStringLen(ad_sid));
  ::SysFreeString(ad_sid);

  HRESULT gaia_id_hr =
      GetUserProperty(sid_str.c_str(), kUserId, gaia_id, &length);
  ASSERT_EQ(S_OK, gaia_id_hr);
  ASSERT_TRUE(gaia_id[0]);

  // Verify that the authentication results dictionary is now empty.
  ASSERT_TRUE(test->IsAuthenticationResultsEmpty());
}

// Tests various sign in scenarios with consumer and non-consumer domains.
// Parameters are:
// 1. Is mdm enrollment enabled.
// 2. The mdm_aca reg key setting:
//    - 0: Set reg key to 0.
//    - 1: Set reg key to 1.
//    - 2: Don't set reg key.
// 3. Whether the mdm_aca reg key is set to 1 or 0.
// 4. Whether an existing associated user is already present.
// 5. Whether the user being created (or existing) uses a consumer account.
class GcpGaiaCredentialBaseConsumerEmailTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<std::tuple<bool, int, bool, bool>> {
};

TEST_P(GcpGaiaCredentialBaseConsumerEmailTest, ConsumerEmailSignin) {
  USES_CONVERSION;
  const bool mdm_enabled = std::get<0>(GetParam());
  const int mdm_consumer_accounts_reg_key_setting = std::get<1>(GetParam());
  const bool user_created = std::get<2>(GetParam());
  const bool user_is_consumer = std::get<3>(GetParam());

  FakeAssociatedUserValidator validator;
  FakeInternetAvailabilityChecker internet_checker;
  GoogleMdmEnrollmentStatusForTesting force_success(true);

  if (mdm_enabled)
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));

  const bool mdm_consumer_accounts_reg_key_set =
      mdm_consumer_accounts_reg_key_setting >= 0 &&
      mdm_consumer_accounts_reg_key_setting < 2;
  if (mdm_consumer_accounts_reg_key_set) {
    ASSERT_EQ(S_OK,
              SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts,
                                      mdm_consumer_accounts_reg_key_setting));
  }

  std::string user_email = user_is_consumer ? kDefaultEmail : "foo@imfl.info";

  CComBSTR sid;
  base::string16 username(user_is_consumer ? L"foo" : L"foo_imfl");

  // Create a fake user that has the same gaia id as the test gaia id.
  if (user_created) {
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        username, L"password", L"name", L"comment",
                        base::UTF8ToUTF16(kDefaultGaiaId),
                        base::UTF8ToUTF16(user_email), &sid));
    ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());
  }

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  test->SetGlsEmailAddress(user_email);

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  bool should_signin_succeed = !mdm_enabled ||
                               (mdm_consumer_accounts_reg_key_set &&
                                mdm_consumer_accounts_reg_key_setting) ||
                               !user_is_consumer;

  // Sign in success.
  if (should_signin_succeed) {
    // User should have been associated.
    EXPECT_EQ(test->GetFinalUsername(), username);
    // Email should be the same as the default one.
    EXPECT_EQ(test->GetFinalEmail(), user_email);

    ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
  } else {
    // Error message concerning invalid domain is sent.
    ASSERT_EQ(S_OK, FinishLogonProcess(false, false,
                                       IDS_DISALLOWED_CONSUMER_EMAIL_BASE));
  }

  if (user_created) {
    // No new user should be created.
    EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());
  } else {
    // New user created only if their domain is valid for the sign in.
    EXPECT_EQ(1ul + (should_signin_succeed ? 1ul : 0ul),
              fake_os_user_manager()->GetUserCount());
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         GcpGaiaCredentialBaseConsumerEmailTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Values(0, 1, 2),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Test password recovery system for various failure success cases.
// Parameters are:
// 1. int - The expected result of the initial public key retrieval for storing
//          the password. Values are 0 - success, 1 - failure, 2 - timeout.
// 2. int - The expected result of the initial public private retrieval for
//          decrypting the password. Values are 0 - success, 1 - failure,
//          2 - timeout.
// 3. int - The expected result of the initial public private retrieval for
//          decrypting the password. Values are 0 - success, 1 - failure,
//          2 - timeout.
class GcpGaiaCredentialBasePasswordRecoveryTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<std::tuple<int, int, int>> {};

TEST_P(GcpGaiaCredentialBasePasswordRecoveryTest, PasswordRecovery) {
  // Enable standard escrow service features in non-Chrome builds so that
  // the escrow service code can be tested by the build machines.
  GoogleMdmEscrowServiceEnablerForTesting escrow_service_enabler;
  USES_CONVERSION;

  int generate_public_key_result = std::get<0>(GetParam());
  int get_private_key_result = std::get<1>(GetParam());
  int generate_public_key_again_result = std::get<2>(GetParam());

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEscrowServiceServerUrl,
                                          L"https://escrow.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));

  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user associated to a gaia id.
  CComBSTR sid;
  constexpr wchar_t kOldPassword[] = L"password";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                kDefaultUsername, kOldPassword, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Change token response to an invalid one.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);

  // Make a dummy response for successful public key generation and private key
  // retrieval.
  std::string generate_success_response =
      fake_password_recovery_manager()->MakeGenerateKeyPairResponseForTesting(
          kTestPublicKey, kFakeResourceId);

  std::string get_key_success_response =
      fake_password_recovery_manager()->MakeGetPrivateKeyResponseForTesting(
          kTestPrivateKey);

  // Make timeout events for the various escrow service requests if needed.
  std::unique_ptr<base::WaitableEvent> get_key_event;
  std::unique_ptr<base::WaitableEvent> generate_key_event;

  if (generate_public_key_result == 2)
    get_key_event.reset(new base::WaitableEvent());

  if (get_private_key_result == 2)
    generate_key_event.reset(new base::WaitableEvent());

  if (get_key_event || generate_key_event) {
    fake_password_recovery_manager()->SetRequestTimeoutForTesting(
        base::TimeDelta::FromMilliseconds(50));
  }

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_password_recovery_manager()->GetEscrowServiceGenerateKeyPairUrl(),
      FakeWinHttpUrlFetcher::Headers(),
      generate_public_key_result != 1 ? generate_success_response : "{}",
      generate_key_event ? generate_key_event->handle() : INVALID_HANDLE_VALUE);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_password_recovery_manager()->GetEscrowServiceGetPrivateKeyUrl(
          kFakeResourceId),
      FakeWinHttpUrlFetcher::Headers(),
      get_private_key_result != 1 ? get_key_success_response : "{}",
      get_key_event ? get_key_event->handle() : INVALID_HANDLE_VALUE);

  bool should_store_succeed = generate_public_key_result == 0;
  bool should_recover_succeed = get_private_key_result == 0;

  // Sign on once to store the password in the LSA
  {
    // Create provider and start logon.
    Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

    ASSERT_EQ(S_OK, StartLogonProcessAndWait());

    // Finish logon successfully to propagate password recovery information to
    // LSA.
    ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }

  // If there was a timeout for the generation of the public key, signal it now
  // so that the request thread can complete. Also delete the event in case it
  // needs to be used again on the sign in after the password was retrieved.
  if (generate_key_event) {
    generate_key_event->Signal();
    generate_key_event.reset();
  }

  if (generate_public_key_again_result == 2)
    generate_key_event.reset(new base::WaitableEvent());

  if (generate_key_event) {
    fake_password_recovery_manager()->SetRequestTimeoutForTesting(
        base::TimeDelta::FromMilliseconds(50));
  }

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_password_recovery_manager()->GetEscrowServiceGenerateKeyPairUrl(),
      FakeWinHttpUrlFetcher::Headers(),
      generate_public_key_again_result != 1 ? generate_success_response : "{}",
      generate_key_event ? generate_key_event->handle() : INVALID_HANDLE_VALUE);

  constexpr char kNewPassword[] = "password2";

  // Sign in a second time with a different password and see if it is updated
  // automatically.
  {
    // Create provider and start logon.
    Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

    Microsoft::WRL::ComPtr<ITestCredential> test;
    ASSERT_EQ(S_OK, cred.As(&test));

    // Send back a different gaia password to force a password update.
    ASSERT_EQ(S_OK, test->SetGlsGaiaPassword(kNewPassword));

    // Don't send a forced e-mail. It will be sent from the user that was
    // updated during the last sign in.
    ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));

    ASSERT_EQ(S_OK, StartLogonProcessAndWait());

    Microsoft::WRL::ComPtr<ITestCredentialProvider> test_provider;
    ASSERT_EQ(S_OK, created_provider().As(&test_provider));

    // If either password storage or recovery failed then the user will need to
    // enter their old Windows password.
    if (!should_store_succeed || !should_recover_succeed) {
      // Logon should not complete but there is no error message.
      EXPECT_EQ(test_provider->credentials_changed_fired(), false);

      // Make sure password textbox is shown if the recovery of the password
      // through escros service fails.
      ASSERT_EQ(CPFS_DISPLAY_IN_SELECTED_TILE,
                fake_credential_provider_credential_events()->GetFieldState(
                    cred.Get(), FID_CURRENT_PASSWORD_FIELD));

      // Set the correct old password so that the user can sign in.
      ASSERT_EQ(S_OK,
                cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, kOldPassword));

      // Finish logon successfully now which should update the password.
      ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
    } else {
      // Make sure password textbox isn't shown if the recovery of the password
      // through escrow service succeeds.
      ASSERT_EQ(CPFS_HIDDEN,
                fake_credential_provider_credential_events()->GetFieldState(
                    cred.Get(), FID_CURRENT_PASSWORD_FIELD));

      // Make sure the new password is sent to the provider.
      EXPECT_STREQ(A2OLE(kNewPassword), OLE2CW(test_provider->password()));

      // Finish logon successfully but with no credential changed event.
      ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
    }

    // Make sure the user has the new password internally.
    EXPECT_EQ(S_OK, fake_os_user_manager()->IsWindowsPasswordValid(
                        OSUserManager::GetLocalDomain().c_str(),
                        kDefaultUsername, A2OLE(kNewPassword)));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }

  // Complete the private key retrieval request if it was waiting.
  if (get_key_event)
    get_key_event->Signal();

  // If generate of the second public key failed, the next sign in would
  // need to re-enter their password
  if (generate_public_key_again_result != 0) {
    constexpr char kNewPassword2[] = "password3";
    // Create provider and start logon.
    Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

    Microsoft::WRL::ComPtr<ITestCredential> test;
    ASSERT_EQ(S_OK, cred.As(&test));

    // Send back a different gaia password to force a password update.
    ASSERT_EQ(S_OK, test->SetGlsGaiaPassword(kNewPassword2));

    // Don't send a forced e-mail. It will be sent from the user that was
    // updated during the last sign in.
    ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));

    ASSERT_EQ(S_OK, StartLogonProcessAndWait());

    Microsoft::WRL::ComPtr<ITestCredentialProvider> test_provider;
    ASSERT_EQ(S_OK, created_provider().As(&test_provider));

    // Logon should not complete but there is no error message.
    EXPECT_EQ(test_provider->credentials_changed_fired(), false);

    // Set the correct old password so that the user can sign in.
    ASSERT_EQ(S_OK,
              cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD,
                                   base::UTF8ToUTF16(kNewPassword).c_str()));

    // Finish logon successfully now which should update the password.
    ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));

    // Make sure the user has the new password internally.
    EXPECT_EQ(S_OK, fake_os_user_manager()->IsWindowsPasswordValid(
                        OSUserManager::GetLocalDomain().c_str(),
                        kDefaultUsername, A2OLE(kNewPassword2)));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         GcpGaiaCredentialBasePasswordRecoveryTest,
                         ::testing::Combine(::testing::Values(0, 1, 2),
                                            ::testing::Values(0, 1, 2),
                                            ::testing::Values(0, 1, 2)));

// Test password recovery system being disabled by registry settings.
// Parameter is a pointer to an escrow service url. Can be empty or nullptr.
class GcpGaiaCredentialBasePasswordRecoveryDisablingTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<const wchar_t*> {};

TEST_P(GcpGaiaCredentialBasePasswordRecoveryDisablingTest,
       PasswordRecovery_Disabled) {
  // Enable standard escrow service features in non-Chrome builds so that
  // the escrow service code can be tested by the build machines.
  GoogleMdmEscrowServiceEnablerForTesting escrow_service_enabler;
  USES_CONVERSION;
  const wchar_t* escrow_service_url = GetParam();

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));
  // SetGlobalFlagForTesting effectively deletes the registry when the provided
  // registry value is empty. That implicitly enables escrow service without a
  // registry override.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEscrowServiceServerUrl, L""));

  if (escrow_service_url) {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(HKEY_LOCAL_MACHINE, kGcpRootKeyName, KEY_WRITE));
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(kRegEscrowServiceServerUrl, escrow_service_url));
  }

  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user associated to a gaia id.
  CComBSTR sid;
  constexpr wchar_t kOldPassword[] = L"password";
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                kDefaultUsername, kOldPassword, L"Full Name", L"comment",
                base::UTF8ToUTF16(kDefaultGaiaId), base::string16(), &sid));

  // Change token response to an invalid one.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);

  // Make a dummy response for successful public key generation and private key
  // retrieval.
  std::string generate_success_response =
      fake_password_recovery_manager()->MakeGenerateKeyPairResponseForTesting(
          kTestPublicKey, kFakeResourceId);

  std::string get_key_success_response =
      fake_password_recovery_manager()->MakeGetPrivateKeyResponseForTesting(
          kTestPrivateKey);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_password_recovery_manager()->GetEscrowServiceGenerateKeyPairUrl(),
      FakeWinHttpUrlFetcher::Headers(), generate_success_response);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_password_recovery_manager()->GetEscrowServiceGetPrivateKeyUrl(
          kFakeResourceId),
      FakeWinHttpUrlFetcher::Headers(), get_key_success_response);

  // Sign on once to store the password in the LSA
  {
    // Create provider and start logon.
    Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

    ASSERT_EQ(S_OK, StartLogonProcessAndWait());

    // Finish logon successfully to propagate password recovery information to
    // LSA.
    ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }

  // Sign in a second time with a different password and see if it is updated
  // automatically.
  {
    constexpr char kNewPassword[] = "password2";

    // Create provider and start logon.
    Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

    Microsoft::WRL::ComPtr<ITestCredential> test;
    ASSERT_EQ(S_OK, cred.As(&test));

    // Send back a different gaia password to force a password update.
    ASSERT_EQ(S_OK, test->SetGlsGaiaPassword(kNewPassword));

    // Don't send a forced e-mail. It will be sent from the user that was
    // updated during the last sign in.
    ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));

    ASSERT_EQ(S_OK, StartLogonProcessAndWait());

    Microsoft::WRL::ComPtr<ITestCredentialProvider> test_provider;
    ASSERT_EQ(S_OK, created_provider().As(&test_provider));

    // Empty escrow service url will disable password
    // recovery and force the user to enter their password.
    if (escrow_service_url && escrow_service_url[0] == '\0') {
      // Logon should not complete but there is no error message.
      EXPECT_EQ(test_provider->credentials_changed_fired(), false);

      // Set the correct old password so that the user can sign in.
      ASSERT_EQ(S_OK,
                cred->SetStringValue(FID_CURRENT_PASSWORD_FIELD, kOldPassword));

      // Finish logon successfully now which should update the password.
      ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));
    } else {
      // Make sure the new password is sent to the provider.
      EXPECT_STREQ(A2OLE(kNewPassword), OLE2CW(test_provider->password()));

      // Finish logon successfully but with no credential changed event.
      ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
    }

    // Make sure the user has the new password internally.
    EXPECT_EQ(S_OK, fake_os_user_manager()->IsWindowsPasswordValid(
                        OSUserManager::GetLocalDomain().c_str(),
                        kDefaultUsername, A2OLE(kNewPassword)));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         GcpGaiaCredentialBasePasswordRecoveryDisablingTest,
                         ::testing::Values(nullptr,
                                           L"",
                                           L"https://escrowservice.com"));

TEST_F(GcpGaiaCredentialBaseTest, FullNameUpdated) {
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
                L"comment", base::UTF8ToUTF16(test_data_storage.GetSuccessId()),
                OLE2CW(email), &sid));

  base::string16 current_full_name;
  ASSERT_EQ(S_OK, OSUserManager::Get()->GetUserFullname(
                      OSUserManager::GetLocalDomain().c_str(), username,
                      &current_full_name));
  ASSERT_EQ(current_full_name, (BSTR)full_name);

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response so that a reauth occurs.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(1, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(std::string()));

  // Override the full name in the gls command line.
  std::string new_full_name = "New Name";
  ASSERT_EQ(S_OK, test->SetGaiaFullNameOverride(new_full_name));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  base::string16 updated_full_name;
  ASSERT_EQ(S_OK, OSUserManager::Get()->GetUserFullname(
                      OSUserManager::GetLocalDomain().c_str(), username,
                      &updated_full_name));
  ASSERT_EQ(updated_full_name, base::UTF8ToUTF16(new_full_name));
}

}  // namespace testing
}  // namespace credential_provider
