// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"

#include <windows.h>

#include <sddl.h>  // For ConvertSidToStringSid()
#include <wrl/client.h>

#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/base_paths_win.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/uuid.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/password_recovery_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/user_policies_manager.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "google_apis/gaia/gaia_urls.h"
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

// Tests the GetSerialization Finish scenario.
// 1. Is gem features enabled. If enabled, tos should be tested out.
//    Otherwise, ToS shouldn't be set irrespective of the |kAcceptTos|
//    registry entry.
class GcpGaiaCredentialGetSerializationBaseTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(GcpGaiaCredentialGetSerializationBaseTest, Finish) {
  bool is_gem_features_enabled = GetParam();

  if (is_gem_features_enabled) {
    // Set |kKeyEnableGemFeatures| registry entry to 1.
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kKeyEnableGemFeatures, 1u));
  } else {
    // Set |kKeyEnableGemFeatures| registry entry to 0.
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kKeyEnableGemFeatures, 0u));
  }

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
  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

  // Make sure ToS acceptance when is_gem_features_enabled isn't enabled.
  DWORD accept_tos = 0u;
  wchar_t* user_sid_string = nullptr;
  ASSERT_TRUE(ConvertSidToStringSid(sid, &user_sid_string));
  HRESULT hr = GetUserProperty(user_sid_string, kKeyAcceptTos, &accept_tos);
  if (is_gem_features_enabled) {
    ASSERT_EQ(S_OK, hr);
    ASSERT_EQ(1u, accept_tos);
  } else {
    ASSERT_TRUE(FAILED(hr));
    ASSERT_EQ(0u, accept_tos);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpGaiaCredentialGetSerializationBaseTest,
                         ::testing::Values(true, false));

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
  ULONG length = std::size(gaia_id);
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
  std::wstring username(L"foo");
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                username, L"password", L"name", L"comment",
                base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &first_sid));
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
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));

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

INSTANTIATE_TEST_SUITE_P(All,
                         GcpGaiaCredentialBaseForceResetRegistryTest,
                         ::testing::Values(0, 1, 2));

TEST_F(GcpGaiaCredentialBaseTest,
       GetSerialization_PasswordChangedForAssociatedUser) {
  USES_CONVERSION;

  // Create a fake user for which the windows password does not match the gaia
  // password supplied by the test gls process.
  CComBSTR sid;
  CComBSTR windows_password = L"password2";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));

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
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));

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
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo", (BSTR)windows_password, L"Full Name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));

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
  fake_os_user_manager()->SetFailureReason(FAILEDOPERATIONS::ADD_USER, E_FAIL);

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Logon process should fail with an internal error.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false, IDS_INTERNAL_ERROR_BASE));
}

TEST_F(GcpGaiaCredentialBaseTest, FailedUserCreation_PasswordTooShort) {
  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  // Fail user creation.
  fake_os_user_manager()->SetFailureReason(
      FAILEDOPERATIONS::ADD_USER, HRESULT_FROM_WIN32(NERR_PasswordTooShort));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Logon process should fail with an internal error.
  ASSERT_EQ(S_OK, FinishLogonProcess(false, false,
                                     IDS_CREATE_USER_PASSWORD_TOO_SHORT_BASE));
}

class GcpGaiaCredentialBaseInvalidDomainTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<const wchar_t*, const wchar_t*>> {
};

TEST_P(GcpGaiaCredentialBaseInvalidDomainTest, Fail) {
  // Setting those registry keys to empty string effectively deletes them.
  SetGlobalFlagForTesting(L"ed", L"");
  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"");

  const wchar_t* allow_domains_key = std::get<0>(GetParam());
  const std::wstring allowed_email_domains = std::get<1>(GetParam());
  ASSERT_EQ(S_OK,
            SetGlobalFlagForTesting(allow_domains_key, allowed_email_domains));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));
  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  if (allowed_email_domains.empty()) {
    // Fails due to missing registry key for allowed domains.
    ASSERT_EQ(S_OK,
              StartLogonProcess(/*succeeds=*/false, IDS_EMAIL_MISMATCH_BASE));
  } else {
    ASSERT_EQ(S_OK, StartLogonProcessAndWait());

    std::wstring expected_error_msg =
        GetStringResource(IDS_INVALID_EMAIL_DOMAIN_BASE);

    // Logon process should fail with the specified error message.
    ASSERT_EQ(S_OK, FinishLogonProcess(false, false, expected_error_msg));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpGaiaCredentialBaseInvalidDomainTest,
    ::testing::Combine(::testing::Values(L"ed", L"domains_allowed_to_login"),
                       ::testing::Values(L"",
                                         L"acme.com,acme2.com,acme3.com")));

class GcpGaiaCredentialBasePermittedAccountTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<const wchar_t*, const wchar_t*>> {
};

TEST_P(GcpGaiaCredentialBasePermittedAccountTest, PermittedAccounts) {
  const std::wstring permitted_acounts = std::get<0>(GetParam());
  const std::wstring restricted_domains = std::get<1>(GetParam());

  ASSERT_EQ(S_OK,
            SetGlobalFlagForTesting(L"permitted_accounts", permitted_acounts));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(L"domains_allowed_to_login",
                                          restricted_domains));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));
  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  std::wstring email = L"user@test.com";
  std::wstring email_domain = email.substr(email.find(L"@") + 1);

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(base::WideToUTF8(email)));

  bool allowed_email = permitted_acounts.empty() ||
                       permitted_acounts.find(email) != std::wstring::npos;
  bool found_domain =
      restricted_domains.find(email_domain) != std::wstring::npos;

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  if (allowed_email && found_domain) {
    ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
  } else {
    std::wstring expected_error_msg;
    if (!found_domain) {
      expected_error_msg = GetStringResource(IDS_INVALID_EMAIL_DOMAIN_BASE);
    } else {
      expected_error_msg = GetStringResource(IDS_EMAIL_MISMATCH_BASE);
    }
    // Logon process should fail with the specified error message.
    ASSERT_EQ(S_OK, FinishLogonProcess(false, false, expected_error_msg));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpGaiaCredentialBasePermittedAccountTest,
    ::testing::Combine(
        ::testing::Values(L"",
                          L"user@test.com",
                          L"other@test.com",
                          L"other@test.com,user@test.com"),
        ::testing::Values(L"test.com", L"best.com", L"test.com,best.com")));

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD) {
  USES_CONVERSION;

  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"imfl.info");

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

TEST_F(GcpGaiaCredentialBaseTest, TrimPeriodAtTheEnd) {
  USES_CONVERSION;

  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"abcd.ef.info");

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  // The top level domain("info" in this example) is removed and the rest is
  // truncated to be 20 characters. However, in this example, this will result
  // with "abcdefghijklmn_abcd." which isn't valid per Microsoft documentation.
  // The rule says there shouldn't be a '.' at the end. Thus it needs to be
  // removed.
  constexpr char email[] = "abcdefghijklmn@abcd.ef.info";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"abcdefghijklmn_abcd"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, UseShorterFormForAccountName) {
  USES_CONVERSION;

  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"def.com");

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegUseShorterAccountName, 1));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  constexpr char email[] = "abc@def.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"abc"), test->GetFinalUsername());
  EXPECT_EQ(test->GetFinalEmail(), email);
}

TEST_F(GcpGaiaCredentialBaseTest, UseShorterFormForAccountNameWithConflict) {
  USES_CONVERSION;

  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"def.com");

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegUseShorterAccountName, 1));

  const wchar_t user_name[] = L"abc";
  const wchar_t password[] = L"password";

  CComBSTR local_sid;
  DWORD error;
  HRESULT hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, &local_sid, &error);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(0u, error);

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  constexpr char email[] = "abc@def.com";

  ASSERT_EQ(S_OK, test->SetGlsEmailAddress(email));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_STREQ(W2COLE(L"abc2"), test->GetFinalUsername());
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
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));
  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user that is already associated so when the user tries to
  // sign on and create a new user, it fails.
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo_registered", L"password", L"name", L"comment",
                      L"gaia-id-registered", std::wstring(), &sid));

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
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  GoogleMdmEnrollmentStatusForTesting force_success(true);

  USES_CONVERSION;
  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR sid;
  std::wstring username(L"foo");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      username, L"password", L"name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  // User should have invalid token handle and be locked.
  EXPECT_TRUE(
      fake_associated_user_validator()->IsAuthEnforcedForUser(OLE2W(sid)));
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

TEST_F(GcpGaiaCredentialBaseTest, SigninNotBlockedWhenValidChromeNotFound) {
  // Enforce token handle verification with user locking when the token handle
  // is not valid.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  GoogleMdmEnrollmentStatusForTesting force_success(true);

  // Simulate a valid Chrome installation not being found.
  fake_chrome_checker()->SetHasSupportedChrome(
      FakeChromeAvailabilityChecker::kChromeForceNo);

  USES_CONVERSION;
  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR sid;
  std::wstring username(L"foo");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      username, L"password", L"name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  // Create with invalid token handle response.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(E_FAIL, InitializeProviderAndGetCredential(0, &cred));
}

TEST_F(GcpGaiaCredentialBaseTest, DenySigninBlockedDuringSignin) {
  USES_CONVERSION;

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  GoogleMdmEnrolledStatusForTesting force_success(true);
  GoogleUploadDeviceDetailsNeededForTesting upload_device_details_needed(false);
  FakeUserPoliciesManager fake_user_policies_manager;

  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR first_sid;
  std::wstring username(L"foo");
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                username, L"password", L"name", L"comment",
                base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &first_sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  UserPolicies user_policies;
  fake_user_policies_manager.SetUserPolicies((BSTR)first_sid, user_policies);
  fake_user_policies_manager.SetUserPolicyStaleOrMissing((BSTR)first_sid,
                                                         false);

  std::vector<std::wstring> reauth_sids;
  reauth_sids.push_back((BSTR)first_sid);

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
  EXPECT_FALSE(
      fake_associated_user_validator()
          ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON, reauth_sids));
  EXPECT_FALSE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  // Now finish the logon.
  ASSERT_EQ(S_OK, FinishLogonProcessWithCred(true, true, 0, cred));

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  // Result has not been reported yet, user signin should still not be denied.
  EXPECT_FALSE(
      fake_associated_user_validator()
          ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON, reauth_sids));
  EXPECT_FALSE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  ReportLogonProcessResult(cred);

  // Now signin can be denied for the user if their token handle is invalid.
  EXPECT_TRUE(
      fake_associated_user_validator()
          ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON, reauth_sids));
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

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  GoogleUploadDeviceDetailsNeededForTesting upload_device_details_needed(false);
  FakeUserPoliciesManager fake_user_policies_manager;

  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR first_sid;
  std::wstring username(L"foo");
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                username, L"password", L"name", L"comment",
                base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &first_sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  std::vector<std::wstring> reauth_sids;
  reauth_sids.push_back((BSTR)first_sid);

  // Set the current time same as last token valid timestamp.
  base::Time last_token_valid = base::Time::Now();
  std::wstring last_token_valid_millis = base::NumberToWString(
      last_token_valid.ToDeltaSinceWindowsEpoch().InMilliseconds());
  int validity_period_in_days = 10;
  DWORD validity_period_in_days_dword =
      static_cast<DWORD>(validity_period_in_days);
  ASSERT_EQ(S_OK,
            SetUserProperty((BSTR)first_sid,
                            base::UTF8ToWide(std::string(kKeyLastTokenValid)),
                            last_token_valid_millis));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(
                      base::UTF8ToWide(std::string(kKeyValidityPeriodInDays)),
                      validity_period_in_days_dword));

  UserPolicies user_policies;
  user_policies.validity_period_days = validity_period_in_days_dword;
  fake_user_policies_manager.SetUserPolicies((BSTR)first_sid, user_policies);
  fake_user_policies_manager.SetUserPolicyStaleOrMissing((BSTR)first_sid,
                                                         false);

  GoogleMdmEnrolledStatusForTesting force_success(true);
  fake_internet_checker()->SetHasInternetConnection(
      FakeInternetAvailabilityChecker::kHicForceYes);

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  SetDefaultTokenHandleResponse(kDefaultValidTokenHandleResponse);

  // Create with valid token handle response and sign in the anonymous
  // credential with the user that should still be valid.
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  // User access shouldn't be blocked before login starts.
  EXPECT_FALSE(
      fake_associated_user_validator()
          ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON, reauth_sids));
  EXPECT_FALSE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  // Internet should be disabled for stale online login verifications to be
  // considered.
  SetDefaultTokenHandleResponse(kDefaultInvalidTokenHandleResponse);
  fake_internet_checker()->SetHasInternetConnection(
      FakeInternetAvailabilityChecker::kHicForceNo);
  // Advance the time that is more than the offline validity period.
  BaseTimeClockOverrideValue::current_time_ =
      base::Time::Now() + base::Days(validity_period_in_days) +
      base::Milliseconds(1);
  base::subtle::ScopedTimeClockOverrides time_override(
      &BaseTimeClockOverrideValue::NowOverride, nullptr, nullptr);

  // User access should be blocked now that the time has been moved.
  ASSERT_TRUE(
      fake_associated_user_validator()
          ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON, reauth_sids));
  EXPECT_TRUE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  // Reset the internet back to being on.
  fake_internet_checker()->SetHasInternetConnection(
      FakeInternetAvailabilityChecker::kHicForceYes);

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Now finish the logon.
  ASSERT_EQ(S_OK, FinishLogonProcessWithCred(true, true, 0, cred));

  // User should have been associated.
  EXPECT_EQ(test->GetFinalUsername(), username);
  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  ReportLogonProcessResult(cred);

  // User access shouldn't be blocked after login completes.
  EXPECT_FALSE(
      fake_associated_user_validator()
          ->DenySigninForUsersWithInvalidTokenHandles(CPUS_LOGON, reauth_sids));
  EXPECT_FALSE(fake_associated_user_validator()->IsUserAccessBlockedForTesting(
      OLE2W(first_sid)));

  wchar_t latest_token_valid_millis[512];
  ULONG latest_token_valid_size = std::size(latest_token_valid_millis);
  ASSERT_EQ(S_OK, GetUserProperty(
                      OLE2W(first_sid), base::UTF8ToWide(kKeyLastTokenValid),
                      latest_token_valid_millis, &latest_token_valid_size));
  int64_t latest_token_valid_millis_int64;
  base::StringToInt64(latest_token_valid_millis,
                      &latest_token_valid_millis_int64);

  long difference =
      latest_token_valid_millis_int64 -
      BaseTimeClockOverrideValue::current_time_.ToDeltaSinceWindowsEpoch()
          .InMilliseconds();
  ASSERT_EQ(0, difference);
}

TEST_F(GcpGaiaCredentialBaseTest, StripEmailTLD_Gmail) {
  USES_CONVERSION;

  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"gmail.com");

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

  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"googlemail.com");

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

  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"gmail.com");

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

  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"gmail.com");

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

  SetGlobalFlagForTesting(L"domains_allowed_to_login",
                          L"areallylongdomaindude.com");

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

TEST_F(GcpGaiaCredentialBaseTest, EmailIsAtCom) {
  USES_CONVERSION;

  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"com");

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

  SetGlobalFlagForTesting(L"domains_allowed_to_login", L".com");

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

// Test various existing local account mapping or active directory account
// mapping in cloud sign-in scenarios.
class GcpGaiaCredentialBaseCloudMappingTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override;

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred_;
  // The admin sdk users directory get URL.
  std::string get_cd_user_url_ = base::StringPrintf(
      "https://www.googleapis.com/admin/directory/v1/users/"
      "%s?projection=full&viewType=domain_public",
      base::EscapeUrlEncodedData(kDefaultEmail, true).c_str());
  raw_ptr<GaiaUrls> gaia_urls_ = GaiaUrls::GetInstance();
  bool is_ad_user = GetParam();
};

void GcpGaiaCredentialBaseCloudMappingTest::SetUp() {
  GcpGaiaCredentialBaseTest::SetUp();
  if (is_ad_user) {
    // Set the device as a domain joined machine.
    fake_os_user_manager()->SetIsDeviceDomainJoined(true);
  }

  // Override registry to enable cloud association with google.
  constexpr wchar_t kRegCloudAssociation[] = L"enable_cloud_association";
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegCloudAssociation, 1));

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred_));
}

// Fetching downscoped access token required for calling admin sdk failed.
// The login attempt would fail in this scenario.
TEST_P(GcpGaiaCredentialBaseCloudMappingTest,
       GetSerialization_CallToFetchDownscopedAccessTokenFailed) {
  // Attempt to fetch the token from gaia fails.
  fake_http_url_fetcher_factory()->SetFakeFailedResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()), E_FAIL);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_TRUE(std::size(test->GetFinalEmail()) == 0);

  // Make sure no user was created and the login attempt failed.
  PSID sid = nullptr;
  EXPECT_EQ(
      HRESULT_FROM_WIN32(NERR_UserNotFound),
      fake_os_user_manager()->GetUserSID(
          OSUserManager::GetLocalDomain().c_str(), kDefaultUsername, &sid));
  ASSERT_EQ(nullptr, sid);

  // No new user is created.
  EXPECT_EQ(1ul, fake_os_user_manager()->GetUserCount());

  // TODO(crbug.com/40632675): Set the error message appropriately for failure
  // scenarios.
  ASSERT_EQ(S_OK, FinishLogonProcess(
                      /*expected_success=*/false,
                      /*expected_credentials_change_fired=*/false,
                      IDS_INTERNAL_ERROR_BASE));
}

// Empty access token returned.
TEST_P(GcpGaiaCredentialBaseCloudMappingTest,
       GetSerialization_EmptyAccessTokenReturned) {
  // Set token result to not contain any access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{}");

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_TRUE(std::size(test->GetFinalEmail()) == 0);

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

// Empty AD_accounts or Local_Windows_accounts is returned via admin sdk.
TEST_P(GcpGaiaCredentialBaseCloudMappingTest,
       GetSerialization_NoUserNameFoundFromAdminSdk) {
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

// Call to the admin sdk to fetch AD_accounts or Local_Windows_accounts failed.
TEST_P(GcpGaiaCredentialBaseCloudMappingTest,
       GetSerialization_CallToAdminSdkFailed) {
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

  ASSERT_TRUE(std::size(test->GetFinalEmail()) == 0);

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

INSTANTIATE_TEST_SUITE_P(All,
                         GcpGaiaCredentialBaseCloudMappingTest,
                         ::testing::Values(true, false));

// Test various active directory specific sign in scenarios.
class GcpGaiaCredentialBaseAdScenariosTest : public GcpGaiaCredentialBaseTest {
 protected:
  void SetUp() override;

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred_;
  // The admin sdk users directory get URL.
  std::string get_cd_user_url_ = base::StringPrintf(
      "https://www.googleapis.com/admin/directory/v1/users/"
      "%s?projection=full&viewType=domain_public",
      base::EscapeUrlEncodedData(kDefaultEmail, true).c_str());
  raw_ptr<GaiaUrls> gaia_urls_ = GaiaUrls::GetInstance();
};

void GcpGaiaCredentialBaseAdScenariosTest::SetUp() {
  GcpGaiaCredentialBaseTest::SetUp();

  // Set the device as a domain joined machine.
  fake_os_user_manager()->SetIsDeviceDomainJoined(true);

  // Override registry to enable cloud association with google.
  constexpr wchar_t kRegCloudAssociation[] = L"enable_cloud_association";
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegCloudAssociation, 1));
  // Set |kKeyEnableGemFeatures| registry entry
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kKeyEnableGemFeatures, 1u));

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred_));
}

// Customer configured invalid AD_accounts.
TEST_F(GcpGaiaCredentialBaseAdScenariosTest,
       GetSerialization_WithAD_InvalidADUPNConfigured) {
  // Add the user as a domain joined user.
  const wchar_t user_name[] = L"ad_user";
  const wchar_t password[] = L"password";

  const wchar_t domain_name[] = L"ad_domain";
  CComBSTR existing_user_sid;
  DWORD error;
  HRESULT add_domain_user_hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, domain_name,
      &existing_user_sid, &error);
  ASSERT_EQ(S_OK, add_domain_user_hr);
  ASSERT_EQ(0u, error);

  // Set token result a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  // Invalid configuration in admin sdk. Don't set the username.
  std::string admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": {\"AD_accounts\":"
      "[{ \"value\": \"%ls\\\\\" }]}}}",
      domain_name);
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_TRUE(std::size(test->GetFinalEmail()) == 0);

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

// Customer configured a valid AD UPN but user is trying to a
// machine that is joined to different AD domain forest.
TEST_F(GcpGaiaCredentialBaseAdScenariosTest,
       GetSerialization_WithAD_InvalidDomainForest) {
  // Add the user as a domain joined user.
  const wchar_t user_name[] = L"ad_user";
  const wchar_t password[] = L"password";

  const wchar_t domain_name[] = L"ad_domain";
  CComBSTR existing_user_sid;
  DWORD error;
  HRESULT add_domain_user_hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, domain_name,
      &existing_user_sid, &error);
  ASSERT_EQ(S_OK, add_domain_user_hr);
  ASSERT_EQ(0u, error);

  // Set token result a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  const wchar_t another_domain_name[] = L"ad_another_domain";
  // Invalid configuration in admin sdk. Don't set the username.
  std::string admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": {\"AD_accounts\":"
      "[{ \"value\": \"%ls\\\\%ls\" }]}}}",
      another_domain_name, user_name);
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_TRUE(std::size(test->GetFinalEmail()) == 0);

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
// TODO(crbug.com/327170900): Test is flaky on win-asan.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_GetSerialization_WithADSuccessScenario \
  DISABLED_GetSerialization_WithADSuccessScenario
#else
#define MAYBE_GetSerialization_WithADSuccessScenario \
  GetSerialization_WithADSuccessScenario
#endif
TEST_F(GcpGaiaCredentialBaseAdScenariosTest,
       MAYBE_GetSerialization_WithADSuccessScenario) {
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
      "{\"customSchemas\": {\"Enhanced_desktop_security\": {\"AD_accounts\":"
      "[{ \"value\": \"%ls\\\\%ls\" }]}}}",
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
  ULONG length = std::size(gaia_id);
  std::wstring sid_str(ad_sid, SysStringLen(ad_sid));
  ::SysFreeString(ad_sid);

  HRESULT gaia_id_hr =
      GetUserProperty(sid_str.c_str(), kUserId, gaia_id, &length);
  ASSERT_EQ(S_OK, gaia_id_hr);
  ASSERT_TRUE(gaia_id[0]);

  // Make sure ToS acceptance was recorded.
  DWORD accept_tos;
  HRESULT hr = GetUserProperty(sid_str.c_str(), kKeyAcceptTos, &accept_tos);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(1u, accept_tos);

  // Verify that the registry entry for the domain name was created.
  wchar_t domain_reg[256];
  ULONG domain_reg_length = std::size(domain_reg);
  ASSERT_TRUE(
      SUCCEEDED(GetUserProperty(sid_str.c_str(), base::UTF8ToWide(kKeyDomain),
                                domain_reg, &domain_reg_length)));
  ASSERT_TRUE(domain_reg[0]);
  EXPECT_TRUE(wcscmp(domain_reg, domain_name) == 0);

  // Verify that the registry entry for the username was created.
  wchar_t username_reg[256];
  ULONG username_reg_length = std::size(username_reg);
  ASSERT_TRUE(
      SUCCEEDED(GetUserProperty(sid_str.c_str(), base::UTF8ToWide(kKeyUsername),
                                username_reg, &username_reg_length)));
  ASSERT_TRUE(username_reg[0]);
  EXPECT_TRUE(wcscmp(username_reg, user_name) == 0);

  // Verify that the authentication results dictionary is now empty.
  ASSERT_TRUE(test->IsAuthenticationResultsEmpty());
}

// Test various active directory specific sign in scenarios.
class GcpGaiaCredentialBaseAdOfflineScenariosTest
    : public GcpGaiaCredentialBaseTest {
 protected:
  void SetUp() override;

  // The admin sdk users directory get URL.
  std::string get_cd_user_url_ = base::StringPrintf(
      "https://www.googleapis.com/admin/directory/v1/users/"
      "%s?projection=full&viewType=domain_public",
      base::EscapeUrlEncodedData(kDefaultEmail, true).c_str());
  raw_ptr<GaiaUrls> gaia_urls_ = GaiaUrls::GetInstance();
};

void GcpGaiaCredentialBaseAdOfflineScenariosTest::SetUp() {
  GcpGaiaCredentialBaseTest::SetUp();

  // Set the device as a domain joined machine.
  fake_os_user_manager()->SetIsDeviceDomainJoined(true);

  // Override registry to enable cloud association with google.
  constexpr wchar_t kRegCloudAssociation[] = L"enable_cloud_association";
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegCloudAssociation, 1));
  // Set |kKeyEnableGemFeatures| registry entry
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kKeyEnableGemFeatures, 1u));
}

// Customer configured a valid AD UPN but user is trying to login first time via
// GCPW to an account when domain controller is unreachable.
TEST_F(GcpGaiaCredentialBaseAdOfflineScenariosTest,
       GetSerialization_WithAD_FirstTimeLoginUnreachableDomainController) {
  // Add the user as a domain joined user.
  const wchar_t user_name[] = L"ad_user";
  const wchar_t password[] = L"password";

  const wchar_t domain_name[] = L"ad_domain";
  CComBSTR existing_user_sid;
  DWORD error;
  HRESULT add_domain_user_hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, domain_name,
      &existing_user_sid, &error);
  ASSERT_EQ(S_OK, add_domain_user_hr);
  ASSERT_EQ(0u, error);

  // Set token result a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  // Invalid configuration in admin sdk. Don't set the username.
  std::string admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": {\"AD_accounts\":"
      "[{ \"value\": \"%ls\\\\%ls\" }]}}}",
      domain_name, user_name);
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred_;
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred_));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  fake_os_user_manager()->FailFindUserBySID(existing_user_sid, 1);

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  ASSERT_TRUE(std::size(test->GetFinalEmail()) == 0);

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

// Customer configured a valid AD UPN but user is trying to login subsequent
// times via GCPW to an account when domain controller is unreachable.
TEST_F(GcpGaiaCredentialBaseAdOfflineScenariosTest,
       GetSerialization_WithAD_SubsequentLoginUnreachableDomainController) {
  // Add the user as a domain joined user.
  const wchar_t user_name[] = L"ad_user";
  const wchar_t password[] = L"password";

  const wchar_t domain_name[] = L"ad_domain";
  CComBSTR existing_user_sid;
  DWORD error;
  HRESULT add_domain_user_hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, domain_name,
      &existing_user_sid, &error);
  ASSERT_EQ(S_OK, add_domain_user_hr);
  ASSERT_EQ(0u, error);

  // Set token result a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  // Invalid configuration in admin sdk. Don't set the username.
  std::string admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": {\"AD_accounts\":"
      "[{ \"value\": \"%ls\\\\%ls\" }]}}}",
      domain_name, user_name);
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  // Login first time when DC is online so that the registry fallbacks for
  // username and domain are set.
  {
    Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred_;
    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred_));

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

    ASSERT_EQ(S_OK, ReleaseProvider());
  }

  {
    Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred_;
    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred_));

    Microsoft::WRL::ComPtr<ITestCredential> test;
    ASSERT_EQ(S_OK, cred_.As(&test));

    // Make sure DC lookup fails so that the registry fallback is used.
    fake_os_user_manager()->FailFindUserBySID(existing_user_sid, 1);

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

    ASSERT_EQ(S_OK, ReleaseProvider());
  }
}

// Test various existing local account mapping specific in cloud sign in
// scenarios.
class GcpGaiaCredentialBaseCloudLocalAccountTest
    : public GcpGaiaCredentialBaseTest {
 protected:
  void SetUp() override;

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred_;
  // The admin sdk users directory get URL.
  std::string get_cd_user_url_ = base::StringPrintf(
      "https://www.googleapis.com/admin/directory/v1/users/"
      "%s?projection=full&viewType=domain_public",
      base::EscapeUrlEncodedData(kDefaultEmail, true).c_str());
  raw_ptr<GaiaUrls> gaia_urls_ = GaiaUrls::GetInstance();
};

void GcpGaiaCredentialBaseCloudLocalAccountTest::SetUp() {
  GcpGaiaCredentialBaseTest::SetUp();

  // Override registry to enable cloud association with google.
  constexpr wchar_t kRegCloudAssociation[] = L"enable_cloud_association";
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegCloudAssociation, 1));

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred_));
}

// Customer configured invalid local account info.
TEST_F(GcpGaiaCredentialBaseCloudLocalAccountTest,
       GetSerialization_InvalidLocalAccountInfoConfigured) {
  // Add the user as a local user.
  const wchar_t user_name[] = L"local_user";
  const wchar_t password[] = L"password";

  CComBSTR local_sid;
  DWORD error;
  HRESULT hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, &local_sid, &error);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(0u, error);

  // Set token result a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  // Invalid configuration in admin sdk. Don't set the username.
  std::string admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": "
      "{\"Local_Windows_accounts\":"
      " \"un:abcd\"}}}");
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Make sure new user was created since no valid mapping was found.
  PSID sid = nullptr;
  fake_os_user_manager()->GetUserSID(OSUserManager::GetLocalDomain().c_str(),
                                     kDefaultUsername, &sid);
  ASSERT_NE(nullptr, sid);

  // New user is created.
  EXPECT_EQ(3ul, fake_os_user_manager()->GetUserCount());

  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
}

// Customer configured invalid local account info.
TEST_F(GcpGaiaCredentialBaseCloudLocalAccountTest,
       GetSerialization_InvalidLocalAccountToSerialNumberConfigured) {
  // Set token result a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  // Set a fake serial number.
  std::wstring serial_number = L"1234";
  GoogleRegistrationDataForTesting g_registration_data(serial_number);

  const wchar_t invalid_user_name_1[] = L"invalid_user_name_1";
  const wchar_t invalid_user_name_2[] = L"invalid_user_name_2";

  // Invalid configuration in admin sdk. Don't set valid usernames.
  std::string admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": "
      "{\"Local_Windows_accounts\":"
      "[{ \"value\": \"un:%ls,sn:%ls\" },{ \"value\": \"un:%ls,sn:%ls\"}]}}}",
      invalid_user_name_1, serial_number.c_str(), invalid_user_name_2,
      serial_number.c_str());
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Make sure new user was created since no valid mapping was found.
  PSID sid = nullptr;
  fake_os_user_manager()->GetUserSID(OSUserManager::GetLocalDomain().c_str(),
                                     kDefaultUsername, &sid);
  ASSERT_NE(nullptr, sid);

  // New user is created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
}

TEST_F(GcpGaiaCredentialBaseCloudLocalAccountTest, MultipleLocalAccountInfo) {
  // Add the user as a local user.
  const wchar_t user_name[] = L"local_user";
  const wchar_t password[] = L"password";

  CComBSTR local_sid;
  DWORD error;
  HRESULT hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, &local_sid, &error);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(0u, error);

  // Set token result as a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  std::string admin_sdk_response;
  // Set a fake serial number.
  std::wstring serial_number = L"1234";
  GoogleRegistrationDataForTesting g_registration_data(serial_number);

  const wchar_t another_user_name[] = L"another_local_user";

  // Set valid response from admin sdk with Local_Windows_accounts containing
  // one mapping with "serial_number" in it and another one without
  // serial number.
  admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": "
      "{\"Local_Windows_accounts\":"
      "[{ \"value\": \"un:%ls,sn:%ls\" },{ \"value\": \"un:%ls\"}]}}}",
      user_name, serial_number.c_str(), another_user_name);
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

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
  std::wstring sid_str(local_sid, SysStringLen(local_sid));

  wchar_t gaia_id[256];
  ULONG length = std::size(gaia_id);
  HRESULT gaia_id_hr =
      GetUserProperty(sid_str.c_str(), kUserId, gaia_id, &length);
  ASSERT_EQ(S_OK, gaia_id_hr);
  ASSERT_TRUE(gaia_id[0]);

  // Verify that the authentication results dictionary is now empty.
  ASSERT_TRUE(test->IsAuthenticationResultsEmpty());
}

TEST_F(GcpGaiaCredentialBaseCloudLocalAccountTest,
       InvalidUserToSerialNumberMapping) {
  // Add the user as a local user.
  const wchar_t user_name[] = L"local_user";
  const wchar_t password[] = L"password";

  CComBSTR local_sid;
  DWORD error;
  HRESULT hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, &local_sid, &error);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(0u, error);

  // Set token result as a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  std::string admin_sdk_response;
  // Set a fake serial number.
  std::wstring serial_number = L"1234";
  GoogleRegistrationDataForTesting g_registration_data(serial_number);

  const wchar_t another_user_name1[] = L"another_local_user_1";
  const wchar_t another_user_name2[] = L"another_local_user_2";

  // Set valid response from admin sdk with Local_Windows_accounts containing
  // multiple mappings with matching "serial_number" in it and another
  // one without serial number.
  admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": "
      "{\"Local_Windows_accounts\":"
      "[{ \"value\": \"un:%ls,sn:%ls\" },{ \"value\": \"un:%ls,sn:%ls\" },{ "
      " \"value\": \"un:%ls\" }]}}}",
      another_user_name1, serial_number.c_str(), another_user_name2,
      serial_number.c_str(), user_name);
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

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
  std::wstring sid_str(local_sid, SysStringLen(local_sid));

  wchar_t gaia_id[256];
  ULONG length = std::size(gaia_id);
  HRESULT gaia_id_hr =
      GetUserProperty(sid_str.c_str(), kUserId, gaia_id, &length);
  ASSERT_EQ(S_OK, gaia_id_hr);
  ASSERT_TRUE(gaia_id[0]);

  // Verify that the authentication results dictionary is now empty.
  ASSERT_TRUE(test->IsAuthenticationResultsEmpty());
}

TEST_F(GcpGaiaCredentialBaseCloudLocalAccountTest,
       OnlyOneValidUserToSerialMapping) {
  // Add the user as a local user.
  const wchar_t user_name[] = L"local_user";
  const wchar_t password[] = L"password";

  CComBSTR local_sid;
  DWORD error;
  HRESULT hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, &local_sid, &error);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(0u, error);

  // Set token result as a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  std::string admin_sdk_response;
  // Set a fake serial number.
  std::wstring serial_number = L"1234";
  GoogleRegistrationDataForTesting g_registration_data(serial_number);

  const wchar_t another_user_name1[] = L"another_local_user_1";

  // Set valid response from admin sdk with Local_Windows_accounts containing
  // multiple mappings with matching "serial_number" in it and another
  // one without serial number.
  admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": "
      "{\"Local_Windows_accounts\":"
      "[{ \"value\": \"un:%ls,sn:%ls\" },{ \"value\": \"un:%ls,sn:%ls\" }]}}}",
      another_user_name1, serial_number.c_str(), user_name,
      serial_number.c_str());
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

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
  std::wstring sid_str(local_sid, SysStringLen(local_sid));

  wchar_t gaia_id[256];
  ULONG length = std::size(gaia_id);
  HRESULT gaia_id_hr =
      GetUserProperty(sid_str.c_str(), kUserId, gaia_id, &length);
  ASSERT_EQ(S_OK, gaia_id_hr);
  ASSERT_TRUE(gaia_id[0]);

  // Verify that the authentication results dictionary is now empty.
  ASSERT_TRUE(test->IsAuthenticationResultsEmpty());
}

TEST_F(GcpGaiaCredentialBaseCloudLocalAccountTest, OnlyOneValidUserMapping) {
  // Add the user as a local user.
  const wchar_t user_name[] = L"local_user";
  const wchar_t password[] = L"password";

  CComBSTR local_sid;
  DWORD error;
  HRESULT hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, &local_sid, &error);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(0u, error);

  // Set token result as a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  std::string admin_sdk_response;
  // Set a fake serial number.
  std::wstring serial_number = L"1234";
  GoogleRegistrationDataForTesting g_registration_data(serial_number);

  const wchar_t another_user_name1[] = L"another_local_user_1";

  // Set valid response from admin sdk with Local_Windows_accounts containing
  // multiple mappings with matching "serial_number" in it and another
  // one without serial number.
  admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": "
      "{\"Local_Windows_accounts\":"
      "[{ \"value\": \"un:%ls,sn:%ls\" },{ \"value\": \"un:%ls,sn:%ls\" }]}}}",
      another_user_name1, serial_number.c_str(), user_name,
      serial_number.c_str());
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

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
  std::wstring sid_str(local_sid, SysStringLen(local_sid));

  wchar_t gaia_id[256];
  ULONG length = std::size(gaia_id);
  HRESULT gaia_id_hr =
      GetUserProperty(sid_str.c_str(), kUserId, gaia_id, &length);
  ASSERT_EQ(S_OK, gaia_id_hr);
  ASSERT_TRUE(gaia_id[0]);

  // Verify that the authentication results dictionary is now empty.
  ASSERT_TRUE(test->IsAuthenticationResultsEmpty());
}

TEST_F(GcpGaiaCredentialBaseCloudLocalAccountTest,
       InvalidUsersToSerialNumberMapping) {
  // Add the user as a local user.
  const wchar_t user_name[] = L"local_user";
  const wchar_t password[] = L"password";

  CComBSTR local_sid;
  DWORD error;
  HRESULT hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, &local_sid, &error);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(0u, error);

  // Set token result as a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  std::string admin_sdk_response;
  // Set a fake serial number.
  std::wstring serial_number = L"1234";
  GoogleRegistrationDataForTesting g_registration_data(serial_number);

  const wchar_t another_user_name1[] = L"another_local_user_1";
  const wchar_t another_user_name2[] = L"another_local_user_2";

  // Set valid response from admin sdk with Local_Windows_accounts containing
  // multiple mappings with matching "serial_number" in it and another
  // one without serial number.
  admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": "
      "{\"Local_Windows_accounts\":"
      "[{ \"value\": \"un:%ls,sn:%ls\" },{ \"value\": \"un:%ls,sn:%ls\" },{ "
      " \"value\": \"un:%ls\" }]}}}",
      another_user_name1, serial_number.c_str(), another_user_name2,
      serial_number.c_str(), user_name);
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

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
  std::wstring sid_str(local_sid, SysStringLen(local_sid));

  wchar_t gaia_id[256];
  ULONG length = std::size(gaia_id);
  HRESULT gaia_id_hr =
      GetUserProperty(sid_str.c_str(), kUserId, gaia_id, &length);
  ASSERT_EQ(S_OK, gaia_id_hr);
  ASSERT_TRUE(gaia_id[0]);

  // Verify that the authentication results dictionary is now empty.
  ASSERT_TRUE(test->IsAuthenticationResultsEmpty());
}

TEST_F(GcpGaiaCredentialBaseCloudLocalAccountTest,
       MultipleValidLocalAccountInfoMapping) {
  // Set token result as a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  std::string admin_sdk_response;
  // Set a fake serial number.
  std::wstring serial_number = L"1234";
  GoogleRegistrationDataForTesting g_registration_data(serial_number);

  const wchar_t another_user_name1[] = L"another_local_user_1";
  const wchar_t another_user_name2[] = L"another_local_user_2";

  // Set valid response from admin sdk with Local_Windows_accounts containing
  // multiple mappings with matching "serial_number" in it and multiple
  // mappings without serial number.
  admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": "
      "{\"Local_Windows_accounts\":"
      "[{ \"value\": \"un:%ls,sn:%ls\" },{ \"value\": \"un:%ls,sn:%ls\" },{ "
      " \"value\": \"un:%ls\" },{ \"value\": \"un:%ls\"}]}}}",
      another_user_name1, serial_number.c_str(), another_user_name2,
      serial_number.c_str(), another_user_name1, another_user_name2);
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Make sure new user was created since no valid mapping was found.
  PSID sid = nullptr;
  fake_os_user_manager()->GetUserSID(OSUserManager::GetLocalDomain().c_str(),
                                     kDefaultUsername, &sid);
  ASSERT_NE(nullptr, sid);

  // New user is created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
}

// This is the success scenario where all preconditions are met in the
// existing cloud local account login scenario. The user is successfully
// logged in.
class GaiaCredentialBaseCloudLocalAccountSuccessTest
    : public GcpGaiaCredentialBaseCloudLocalAccountTest,
      public ::testing::WithParamInterface<std::tuple<bool, const wchar_t*>> {};

TEST_P(GaiaCredentialBaseCloudLocalAccountSuccessTest, SerialNumber) {
  bool set_serial_number = std::get<0>(GetParam());
  const wchar_t* serial_number = std::get<1>(GetParam());

  // Add the user as a local user.
  const wchar_t user_name[] = L"local_user";
  const wchar_t password[] = L"password";

  CComBSTR local_sid;
  DWORD error;
  HRESULT hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, &local_sid, &error);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(0u, error);

  // Set token result as a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  std::string admin_sdk_response;
  // Set a fake serial number.
  GoogleRegistrationDataForTesting g_registration_data(serial_number);

  if (set_serial_number) {
    // Set valid response from admin sdk.
    admin_sdk_response = base::StringPrintf(
        "{\"customSchemas\": {\"Enhanced_desktop_security\": "
        "{\"Local_Windows_accounts\":"
        "[{ \"value\": \"un:%ls,sn:%ls\"}]}}}",
        user_name, serial_number);
  } else {
    // Set valid response from admin sdk.
    admin_sdk_response = base::StringPrintf(
        "{\"customSchemas\": {\"Enhanced_desktop_security\": "
        "{\"Local_Windows_accounts\":"
        "[{ \"value\": \"un:%ls\"}]}}}",
        user_name);
  }
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

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
  std::wstring sid_str(local_sid, SysStringLen(local_sid));

  wchar_t gaia_id[256];
  ULONG length = std::size(gaia_id);
  HRESULT gaia_id_hr =
      GetUserProperty(sid_str.c_str(), kUserId, gaia_id, &length);
  ASSERT_EQ(S_OK, gaia_id_hr);
  ASSERT_TRUE(gaia_id[0]);

  // Verify that the authentication results dictionary is now empty.
  ASSERT_TRUE(test->IsAuthenticationResultsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GaiaCredentialBaseCloudLocalAccountSuccessTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(L"!@#!",        // All non alphanumeric characters
                          L"serial#123",  // Contains non-alphanumeric chars.
                          L"serial123!"   // Ends with non alphanumeric chars.
                          )));

// Existing cloud local account login scenario that was configured incorrectly.
class GaiaCredentialBaseCDUsernameSuccessTest
    : public GcpGaiaCredentialBaseCloudLocalAccountTest,
      public ::testing::WithParamInterface<const wchar_t*> {};

TEST_P(GaiaCredentialBaseCDUsernameSuccessTest, AnyUsername) {
  const wchar_t* user_name = GetParam();

  // Add the user as a local user.
  const wchar_t password[] = L"password";

  CComBSTR local_sid;
  DWORD error;
  HRESULT hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, &local_sid, &error);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(0u, error);

  // Set token result as a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  // Set valid response from admin sdk.
  std::string admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": "
      "{\"Local_Windows_accounts\":"
      "[{ \"value\": \"un:%ls\"}]}}}",
      user_name);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // New user is not created.
  EXPECT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

  // Verify that the registry entry for the user was created.
  std::wstring sid_str(local_sid, SysStringLen(local_sid));

  wchar_t gaia_id[256];
  ULONG length = std::size(gaia_id);
  HRESULT gaia_id_hr =
      GetUserProperty(sid_str.c_str(), kUserId, gaia_id, &length);
  ASSERT_EQ(S_OK, gaia_id_hr);
  ASSERT_TRUE(gaia_id[0]);

  // Verify that the authentication results dictionary is now empty.
  ASSERT_TRUE(test->IsAuthenticationResultsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GaiaCredentialBaseCDUsernameSuccessTest,
    ::testing::Values(L"!@#!",        // All non alphanumeric characters
                      L"user#123",    // Contains non-alphanumeric chars.
                      L"user123!"));  // Ends with non alphanumeric chars.

// Existing cloud local account login scenario that was configured incorrectly.
class GaiaCredentialBaseCDSerialNumberFailureTest
    : public GcpGaiaCredentialBaseCloudLocalAccountTest,
      public ::testing::WithParamInterface<const wchar_t*> {};

TEST_P(GaiaCredentialBaseCDSerialNumberFailureTest, InvalidSerialNumber) {
  const wchar_t* serial_number = GetParam();

  // Add the user as a local user.
  const wchar_t user_name[] = L"local_user";
  const wchar_t password[] = L"password";

  CComBSTR local_sid;
  DWORD error;
  HRESULT hr = fake_os_user_manager()->AddUser(
      user_name, password, L"fullname", L"comment", true, &local_sid, &error);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(0u, error);

  // Set fake serial number.
  GoogleRegistrationDataForTesting g_registration_data(serial_number);

  // Set token result as a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(gaia_urls_->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");

  // Set valid response from admin sdk.
  std::string admin_sdk_response = base::StringPrintf(
      "{\"customSchemas\": {\"Enhanced_desktop_security\": "
      "{\"Local_Windows_accounts\":"
      "[{ \"value\": \"un:%ls,sn:%ls\"}]}}}",
      user_name, serial_number);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(get_cd_user_url_.c_str()), FakeWinHttpUrlFetcher::Headers(),
      admin_sdk_response);

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred_.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Make sure new user was created since no valid mapping was found.
  PSID sid = nullptr;
  fake_os_user_manager()->GetUserSID(OSUserManager::GetLocalDomain().c_str(),
                                     kDefaultUsername, &sid);
  ASSERT_NE(nullptr, sid);

  // New user is created.
  EXPECT_EQ(3ul, fake_os_user_manager()->GetUserCount());

  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GaiaCredentialBaseCDSerialNumberFailureTest,
    ::testing::Values(
        L""  // Except for empty string all other characters are allowed chars.
        ));

// Tests various sign in scenarios with consumer and non-consumer domains.
// Parameters are:
// 1. bool : Is mdm enrollment enabled.
// 2. int  : The mdm_aca reg key setting:
//         - 0: Set reg key to 0.
//         - 1: Set reg key to 1.
//         - 2: Don't set reg key.
// 3. bool : Whether an existing associated user is already present.
// 4. bool : Whether the user being created (or existing) uses a consumer
//           account.
// 5. bool : Whether cloud policies are enabled.
class GcpGaiaCredentialBaseConsumerEmailTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, int, bool, bool, bool>> {};

TEST_P(GcpGaiaCredentialBaseConsumerEmailTest, ConsumerEmailSignin) {
  USES_CONVERSION;
  const bool mdm_enabled = std::get<0>(GetParam());
  const int mdm_consumer_accounts_reg_key_setting = std::get<1>(GetParam());
  const bool user_created = std::get<2>(GetParam());
  const bool user_is_consumer = std::get<3>(GetParam());
  const bool cloud_policies_enabled = std::get<4>(GetParam());

  if (user_is_consumer) {
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(L"domains_allowed_to_login",
                                            L"gmail.com"));
  } else {
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(L"domains_allowed_to_login",
                                            L"imfl.info"));
  }

  FakeAssociatedUserValidator validator;
  FakeInternetAvailabilityChecker internet_checker;
  GoogleMdmEnrollmentStatusForTesting force_success(true);
  FakeDevicePoliciesManager fake_device_policies_manager(
      cloud_policies_enabled);

  if (cloud_policies_enabled) {
    DevicePolicies policies;
    policies.enable_dm_enrollment = mdm_enabled;
    fake_device_policies_manager.SetDevicePolicies(policies);
  } else {
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment,
                                            mdm_enabled ? 1 : 0));
  }

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
  std::wstring username(user_is_consumer ? L"foo" : L"foo_imfl");

  // Create a fake user that has the same gaia id as the test gaia id.
  if (user_created) {
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        username, L"password", L"name", L"comment",
                        base::UTF8ToWide(kDefaultGaiaId),
                        base::UTF8ToWide(user_email), &sid));
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

INSTANTIATE_TEST_SUITE_P(All,
                         GcpGaiaCredentialBaseConsumerEmailTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Values(0, 1, 2),
                                            ::testing::Bool(),
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

TEST_P(GcpGaiaCredentialBasePasswordRecoveryTest, DISABLED_PasswordRecovery) {
  USES_CONVERSION;

  int generate_public_key_result = std::get<0>(GetParam());
  int get_private_key_result = std::get<1>(GetParam());
  int generate_public_key_again_result = std::get<2>(GetParam());

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegDisablePasswordSync, 0));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));

  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user associated to a gaia id.
  CComBSTR sid;
  constexpr wchar_t kOldPassword[] = L"password";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      kDefaultUsername, kOldPassword, L"Full Name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));

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
    get_key_event = std::make_unique<base::WaitableEvent>();

  if (get_private_key_result == 2)
    generate_key_event = std::make_unique<base::WaitableEvent>();

  if (get_key_event || generate_key_event) {
    fake_password_recovery_manager()->SetRequestTimeoutForTesting(
        base::Milliseconds(50));
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
    generate_key_event = std::make_unique<base::WaitableEvent>();

  if (generate_key_event) {
    fake_password_recovery_manager()->SetRequestTimeoutForTesting(
        base::Milliseconds(50));
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
                                   base::UTF8ToWide(kNewPassword).c_str()));

    // Finish logon successfully now which should update the password.
    ASSERT_EQ(S_OK, FinishLogonProcess(true, false, 0));

    // Make sure the user has the new password internally.
    EXPECT_EQ(S_OK, fake_os_user_manager()->IsWindowsPasswordValid(
                        OSUserManager::GetLocalDomain().c_str(),
                        kDefaultUsername, A2OLE(kNewPassword2)));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpGaiaCredentialBasePasswordRecoveryTest,
                         ::testing::Combine(::testing::Values(0, 1, 2),
                                            ::testing::Values(0, 1, 2),
                                            ::testing::Values(0, 1, 2)));

// Tests failures in NetUserChangePassword attempt after password is
// successfully retrieved.
// 1. int - Password change attempt fails due to multiple reasons. Values are
//          0 - ERROR_INVALID_PASSWORD, 1 - NERR_InvalidComputer,
//          2 - NERR_NotPrimary, 3 - NERR_UserNotFound, 4 -
//          NERR_PasswordTooShort, 5 - UnknownStatus
class GcpGaiaCredentialBasePasswordChangeFailureTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<int> {};

TEST_P(GcpGaiaCredentialBasePasswordChangeFailureTest, Fail) {
  USES_CONVERSION;

  int failure_reason = GetParam();

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegDisablePasswordSync, 0));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));

  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user associated to a gaia id.
  CComBSTR sid;
  constexpr wchar_t kOldPassword[] = L"password";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      kDefaultUsername, kOldPassword, L"Full Name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));

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
      FakeWinHttpUrlFetcher::Headers(), generate_success_response,
      INVALID_HANDLE_VALUE);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_password_recovery_manager()->GetEscrowServiceGetPrivateKeyUrl(
          kFakeResourceId),
      FakeWinHttpUrlFetcher::Headers(), get_key_success_response,
      INVALID_HANDLE_VALUE);

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

  constexpr char kNewPassword[] = "password2";

  // Sign in a second time with a different password and see if it is updated
  // automatically.
  {
    HRESULT net_api_status;
    UINT message_id;
    switch (failure_reason) {
      case 0:
        net_api_status = HRESULT_FROM_WIN32(ERROR_INVALID_PASSWORD);
        message_id = IDS_INVALID_PASSWORD_BASE;
        break;
      case 1:
        net_api_status = HRESULT_FROM_WIN32(NERR_InvalidComputer);
        message_id = IDS_INVALID_COMPUTER_NAME_ERROR_BASE;
        break;
      case 2:
        net_api_status = HRESULT_FROM_WIN32(NERR_NotPrimary);
        message_id = IDS_AD_PASSWORD_CHANGE_DENIED_BASE;
        break;
      case 3:
        net_api_status = HRESULT_FROM_WIN32(NERR_UserNotFound);
        message_id = IDS_USER_NOT_FOUND_PASSWORD_ERROR_BASE;
        break;
      case 4:
        net_api_status = HRESULT_FROM_WIN32(NERR_PasswordTooShort);
        message_id = IDS_PASSWORD_COMPLEXITY_ERROR_BASE;
        break;
      default:
        net_api_status = E_FAIL;
        message_id = IDS_UNKNOWN_PASSWORD_ERROR_BASE;
        break;
    }

    std::wstring expected_error_msg = GetStringResource(message_id);

    // Set reason for failing the password change attempt.
    fake_os_user_manager()->SetFailureReason(FAILEDOPERATIONS::CHANGE_PASSWORD,
                                             net_api_status);

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

    ASSERT_EQ(net_api_status, FinishLogonProcess(true, true, 0));

    CREDENTIAL_PROVIDER_FIELD_STATE cpfs = CPFS_DISPLAY_IN_SELECTED_TILE;
    if (message_id == IDS_PASSWORD_COMPLEXITY_ERROR_BASE ||
        message_id == IDS_USER_NOT_FOUND_PASSWORD_ERROR_BASE ||
        message_id == IDS_AD_PASSWORD_CHANGE_DENIED_BASE) {
      cpfs = CPFS_HIDDEN;
    }

    // Make sure password textbox is shown due to password change failure.
    ASSERT_EQ(cpfs, fake_credential_provider_credential_events()->GetFieldState(
                        cred.Get(), FID_CURRENT_PASSWORD_FIELD));

    EXPECT_STREQ(expected_error_msg.c_str(),
                 fake_credential_provider_credential_events()->GetFieldString(
                     cred.Get(), FID_DESCRIPTION));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpGaiaCredentialBasePasswordChangeFailureTest,
                         ::testing::Values(0, 1, 2, 3, 4, 5));

// Test password recovery system being disabled by registry settings.
// Parameter is a pointer to an escrow service url. Can be empty or nullptr.
class GcpGaiaCredentialBasePasswordRecoveryDisablingTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<int> {};

TEST_P(GcpGaiaCredentialBasePasswordRecoveryDisablingTest,
       PasswordRecovery_Disabled) {
  USES_CONVERSION;
  int disable_escrow_service = GetParam();

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegEnableDmEnrollment, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));
  // SetGlobalFlagForTesting effectively deletes the registry when the provided
  // registry value is empty. That implicitly enables escrow service without a
  // registry override.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegDisablePasswordSync,
                                          disable_escrow_service));

  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user associated to a gaia id.
  CComBSTR sid;
  constexpr wchar_t kOldPassword[] = L"password";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      kDefaultUsername, kOldPassword, L"Full Name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));

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

    // Disable password recovery and force the user to enter their password.
    if (disable_escrow_service) {
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

INSTANTIATE_TEST_SUITE_P(All,
                         GcpGaiaCredentialBasePasswordRecoveryDisablingTest,
                         ::testing::Values(0, 1));

// Test Upload device details to GEM service with different failure scenarios.
// Parameters are:
// int - 0. Successfully uploaded device details.
//       1. Fails the upload device details call due to network timeout.
//       2. Fails the upload device details call due to invalid response
//          from the GEM http server.
//       3. A previously saved device resource ID is present on the device.
// int - number of previously failed upload device details attempts.
class GcpGaiaCredentialBaseUploadDeviceDetailsTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<std::tuple<int, int>> {};

TEST_P(GcpGaiaCredentialBaseUploadDeviceDetailsTest, UploadDeviceDetails) {
  bool fail_upload_device_details_timeout = (std::get<0>(GetParam()) == 1);
  bool fail_upload_device_details_invalid_response =
      (std::get<0>(GetParam()) == 2);
  bool registry_has_device_resource_id = (std::get<0>(GetParam()) == 3);
  const DWORD num_previous_failures = std::get<1>(GetParam());

  GoogleMdmEnrolledStatusForTesting force_success(true);
  // Set a fake serial number.
  std::wstring serial_number = L"1234";
  GoogleRegistrationDataForTesting g_registration_data(serial_number);
  std::wstring domain = L"domain";
  std::wstring machine_guid = L"machine_guid";
  SetMachineGuidForTesting(machine_guid);

  std::vector<std::string> mac_addresses;
  mac_addresses.push_back("mac_address_1");
  mac_addresses.push_back("mac_address_2");
  std::string os_version = "10.1.17134";
  GemDeviceDetailsForTesting g_device_details(mac_addresses, os_version);

  // Create a fake user associated to a gaia id.
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      kDefaultUsername, L"password", L"Full Name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), domain,
                      &sid));

  std::string dm_token = base::Uuid::GenerateRandomV4().AsLowercaseString();
  FakeTokenGenerator fake_token_generator;
  fake_token_generator.SetTokensForTesting({dm_token});

  // Change token response to an invalid one.
  SetDefaultTokenHandleResponse(kDefaultValidTokenHandleResponse);

  // Make timeout events for the upload device details request if needed.
  std::unique_ptr<base::WaitableEvent> upload_device_details_key_event;

  if (fail_upload_device_details_timeout) {
    upload_device_details_key_event = std::make_unique<base::WaitableEvent>();

    fake_gem_device_details_manager()->SetRequestTimeoutForTesting(
        base::Milliseconds(50));
  }
  const std::string device_resource_id = "test-device-resource-id";
  const std::string valid_server_response =
      "{\"deviceResourceId\": \"" + device_resource_id + "\"}";

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_gem_device_details_manager()->GetGemServiceUploadDeviceDetailsUrl(),
      FakeWinHttpUrlFetcher::Headers(),
      fail_upload_device_details_invalid_response ? "Invalid json response"
                                                  : valid_server_response,
      upload_device_details_key_event
          ? upload_device_details_key_event->handle()
          : INVALID_HANDLE_VALUE);

  if (registry_has_device_resource_id) {
    HRESULT hr = SetUserProperty(sid.Copy(), kRegUserDeviceResourceId,
                                 base::UTF8ToWide(device_resource_id));
    EXPECT_TRUE(SUCCEEDED(hr));
  }

  // Set status and num failures from previous attempts.
  HRESULT hr = SetUserProperty(sid.Copy(), kRegDeviceDetailsUploadStatus,
                               num_previous_failures ? 0 : 1);
  EXPECT_TRUE(SUCCEEDED(hr));

  hr = SetUserProperty(sid.Copy(), kRegDeviceDetailsUploadFailures,
                       num_previous_failures);
  EXPECT_TRUE(SUCCEEDED(hr));

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Finish logon successfully.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

  // Verify that upload device details http call returned back with appropriate
  // status code. Since the login process doesn't get affected by the status of
  // the upload device details process, the login attempt would always succeed
  // irrespective of the upload status.
  hr = fake_gem_device_details_manager()->GetUploadStatusForTesting();
  bool has_upload_failed = (fail_upload_device_details_timeout ||
                            fail_upload_device_details_invalid_response);
  ASSERT_TRUE(has_upload_failed ? FAILED(hr) : SUCCEEDED(hr));

  // Assert on the request parameters sent in the UploadDeviceDetails rpc.
  const base::Value::Dict& request_dict =
      fake_gem_device_details_manager()->GetRequestDictForTesting();
  ASSERT_NE(nullptr, request_dict.FindString("machine_guid"));
  ASSERT_EQ(*request_dict.FindString("machine_guid"),
            base::WideToUTF8(machine_guid));
  ASSERT_NE(nullptr, request_dict.FindString("device_serial_number"));
  ASSERT_EQ(*request_dict.FindString("device_serial_number"),
            base::WideToUTF8(serial_number));
  ASSERT_NE(nullptr, request_dict.FindString("device_domain"));
  ASSERT_EQ(*request_dict.FindString("device_domain"),
            base::WideToUTF8(domain));
  ASSERT_NE(nullptr, request_dict.FindString("account_username"));
  ASSERT_EQ(*request_dict.FindString("account_username"),
            base::WideToUTF8(kDefaultUsername));
  ASSERT_NE(nullptr, request_dict.FindString("user_sid"));
  ASSERT_EQ(*request_dict.FindString("user_sid"), base::WideToUTF8((BSTR)sid));
  ASSERT_NE(nullptr, request_dict.FindString("os_edition"));
  ASSERT_EQ(*request_dict.FindString("os_edition"), os_version);
  ASSERT_TRUE(request_dict.FindBool("is_ad_joined_user").has_value());
  ASSERT_EQ(request_dict.FindBool("is_ad_joined_user").value(), true);
  const base::Value::List* wlan_mac_addr =
      request_dict.FindList("wlan_mac_addr");
  ASSERT_TRUE(wlan_mac_addr);
  ASSERT_EQ(*request_dict.FindString("dm_token"), dm_token);

  std::vector<std::string> actual_mac_address_list;
  for (const base::Value& value : *wlan_mac_addr) {
    ASSERT_TRUE(value.is_string());
    actual_mac_address_list.push_back(value.GetString());
  }

  ASSERT_TRUE(base::ranges::equal(actual_mac_address_list, mac_addresses));

  if (registry_has_device_resource_id) {
    ASSERT_EQ(*request_dict.FindString("device_resource_id"),
              device_resource_id);
  }

  DWORD device_upload_status = 0;
  hr = GetUserProperty(sid.Copy(), kRegDeviceDetailsUploadStatus,
                       &device_upload_status);
  DWORD device_upload_failures = 0;
  hr = GetUserProperty(sid.Copy(), kRegDeviceDetailsUploadFailures,
                       &device_upload_failures);

  if (!fail_upload_device_details_timeout &&
      !fail_upload_device_details_invalid_response) {
    ASSERT_EQ(1UL, device_upload_status);
    ASSERT_EQ(0UL, device_upload_failures);

    wchar_t resource_id[512];
    ULONG resource_id_size = std::size(resource_id);
    hr = GetUserProperty(sid.Copy(), kRegUserDeviceResourceId, resource_id,
                         &resource_id_size);
    ASSERT_TRUE(SUCCEEDED(hr));
    ASSERT_TRUE(resource_id_size > 0);
    ASSERT_EQ(device_resource_id, base::WideToUTF8(resource_id));
  } else {
    ASSERT_EQ(0UL, device_upload_status);
    ASSERT_EQ(num_previous_failures + 1, device_upload_failures);
  }

  ASSERT_EQ(S_OK, ReleaseProvider());
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpGaiaCredentialBaseUploadDeviceDetailsTest,
                         ::testing::Combine(::testing::Values(0, 1, 2, 3),
                                            ::testing::Values(0, 1, 2)));

class GcpGaiaCredentialBaseFullNameUpdateTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<std::tuple<HRESULT, HRESULT>> {};

TEST_P(GcpGaiaCredentialBaseFullNameUpdateTest, FullNameUpdated) {
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

  std::wstring current_full_name;
  ASSERT_EQ(S_OK, fake_os_user_manager()->GetUserFullname(
                      fake_os_user_manager()->GetLocalDomain().c_str(),
                      username, &current_full_name));
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

  HRESULT get_fullname_hr = std::get<0>(GetParam());
  HRESULT set_fullname_hr = std::get<1>(GetParam());
  if (FAILED(get_fullname_hr)) {
    fake_os_user_manager()->SetFailureReason(
        FAILEDOPERATIONS::GET_USER_FULLNAME, get_fullname_hr);
  }
  if (FAILED(set_fullname_hr)) {
    fake_os_user_manager()->SetFailureReason(
        FAILEDOPERATIONS::SET_USER_FULLNAME, set_fullname_hr);
  }

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  fake_os_user_manager()->RestoreOperation(FAILEDOPERATIONS::GET_USER_FULLNAME);
  fake_os_user_manager()->RestoreOperation(FAILEDOPERATIONS::SET_USER_FULLNAME);

  std::wstring updated_full_name;
  ASSERT_EQ(S_OK, fake_os_user_manager()->GetUserFullname(
                      fake_os_user_manager()->GetLocalDomain().c_str(),
                      username, &updated_full_name));
  if (FAILED(get_fullname_hr) || FAILED(set_fullname_hr)) {
    ASSERT_NE(updated_full_name, base::UTF8ToWide(new_full_name));
  } else {
    ASSERT_EQ(updated_full_name, base::UTF8ToWide(new_full_name));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpGaiaCredentialBaseFullNameUpdateTest,
                         ::testing::Combine(::testing::Values(S_OK, E_FAIL),
                                            ::testing::Values(S_OK, E_FAIL)));

// Test event logs upload to GEM service with different failure scenarios.
// Parameters are:
// 1. bool  true:  HTTP call to upload logs succeeds.
//          false: Fails the upload call due to invalid response from the GEM
//                 http server.
// 2. int - The number of fake events to seed the fake event log with.
class GcpGaiaCredentialBaseUploadEventLogsTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<std::tuple<bool, int>> {};

TEST_P(GcpGaiaCredentialBaseUploadEventLogsTest, UploadEventViewerLogs) {
  bool fail_upload_event_logs_invalid_response = std::get<0>(GetParam());
  uint64_t num_events_in_log = std::get<1>(GetParam());

  // The number of events in the fake log that we consider too many to upload
  // in a single request. The fake log events are all 1KB so this would be about
  // 2.5MB of payload.
  const uint64_t max_number_of_events_handled_per_invocation = 2500;

  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create some fake logs.
  std::vector<FakeEventLogsUploadManager::EventLogEntry> logs;
  for (size_t i = 0; i < num_events_in_log; i++) {
    std::wstring data(1024, '0');  // 1KB payload.
    logs.push_back({i + 1,
                    {1000 + i, static_cast<uint32_t>(200 + i)},
                    data,
                    static_cast<uint32_t>(1 + i % 4)});
  }

  FakeEventLogsUploadManager fake_event_logs_upload_manager(logs);

  // Create a fake user associated to a gaia id.
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      kDefaultUsername, L"password", L"Full Name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));

  // Change token response to an valid one.
  SetDefaultTokenHandleResponse(kDefaultValidTokenHandleResponse);

  fake_http_url_fetcher_factory()->SetFakeResponse(
      fake_event_logs_upload_manager.GetGcpwServiceUploadEventViewerLogsUrl(),
      FakeWinHttpUrlFetcher::Headers(),
      fail_upload_event_logs_invalid_response ? "Invalid json response" : "{}");

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Finish logon successfully.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

  // Verify that the upload call returned an appropriate status code.
  // Upload should fail if the server didn't respond appropriately
  // unless there were no event logs to upload at all.
  HRESULT hr = fake_event_logs_upload_manager.GetUploadStatus();
  ASSERT_TRUE((fail_upload_event_logs_invalid_response && num_events_in_log > 0)
                  ? FAILED(hr)
                  : SUCCEEDED(hr));

  if (!fail_upload_event_logs_invalid_response) {
    if (num_events_in_log > max_number_of_events_handled_per_invocation) {
      // In this case we don't expect that all the events are uploaded.
      ASSERT_TRUE(fake_event_logs_upload_manager.GetNumLogsUploaded() > 0);
      ASSERT_TRUE(fake_event_logs_upload_manager.GetNumLogsUploaded() <=
                  num_events_in_log);
    } else {
      ASSERT_EQ(num_events_in_log,
                fake_event_logs_upload_manager.GetNumLogsUploaded());
    }
  }

  ASSERT_EQ(S_OK, ReleaseProvider());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpGaiaCredentialBaseUploadEventLogsTest,
    ::testing::Combine(::testing::Values(true, false),
                       ::testing::Values(0, 2, 1000, 3000)));

// Test if the credential can be created successfully depending on whether a
// Chrome path is found.
// Parameters are:
// 1. bool  true:  A Chrome path is set.
//          false: No Chrome path set.
class GcpGaiaCredentialBaseChromeAvailabilityTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(GcpGaiaCredentialBaseChromeAvailabilityTest, CustomChromeSpecified) {
  // Simulate a custom Chrome path being set.
  fake_chrome_checker()->SetHasSupportedChrome(
      FakeChromeAvailabilityChecker::kChromeDontForce);

  bool custom_path_set = GetParam();
  base::ScopedTempDir temp_chrome_path;

  // Set system Chrome path to empty so that we are not influenced by the
  // runtime environment.
  GoogleChromePathForTesting google_chrome_path_for_testing(
      base::FilePath(L""));

  if (custom_path_set) {
    ASSERT_TRUE(temp_chrome_path.CreateUniqueTempDir());
    ASSERT_EQ(S_OK, SetGlobalFlagForTesting(
                        kRegGlsPath, temp_chrome_path.GetPath().value()));
  }

  USES_CONVERSION;
  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR sid;
  std::wstring username(L"foo");
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      username, L"password", L"name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));
  ASSERT_EQ(2ul, fake_os_user_manager()->GetUserCount());

  // Create provider.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  if (custom_path_set) {
    // Don't fail to create the credential.
    ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));
  } else {
    // Credential creation should fail as no chrome will be found.
    ASSERT_EQ(E_FAIL, InitializeProviderAndGetCredential(0, &cred));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpGaiaCredentialBaseChromeAvailabilityTest,
                         ::testing::Values(true, false));

// Test fetching of user cloud policies from the GEM service with different
// failure scenarios.
// Parameters are:
// 1. bool  true:  HTTP call to fetch policies succeeds.
//          false: Fails the upload call due to invalid response from the GEM
//                 http server.
// 2. bool  true:  Policies were fetched recently and don't need refreshing.
//          false: Policies were never fetched or are very old.
// 3. bool :       Whether cloud policies feature is enabled.
class GcpGaiaCredentialBaseFetchCloudPoliciesTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 protected:
  void SetUp() override;
};

void GcpGaiaCredentialBaseFetchCloudPoliciesTest::SetUp() {
  GcpGaiaCredentialBaseTest::SetUp();

  FakesForTesting fakes;
  fakes.fake_win_http_url_fetcher_creator =
      fake_http_url_fetcher_factory()->GetCreatorCallback();
  UserPoliciesManager::Get()->SetFakesForTesting(&fakes);
}

TEST_P(GcpGaiaCredentialBaseFetchCloudPoliciesTest, FetchAndStore) {
  bool fail_fetch_policies = std::get<0>(GetParam());
  bool policy_refreshed_recently = std::get<1>(GetParam());
  bool cloud_policies_enabled = std::get<2>(GetParam());

  FakeUserPoliciesManager fake_user_policies_manager(cloud_policies_enabled);
  GoogleMdmEnrolledStatusForTesting force_success(true);

  // Create a fake user associated to a gaia id.
  CComBSTR sid_str;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                kDefaultUsername, L"password", L"Full Name", L"comment",
                base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid_str));
  std::wstring sid = OLE2W(sid_str);

  if (cloud_policies_enabled) {
    fake_user_policies_manager.SetUserPolicyStaleOrMissing(
        sid, !policy_refreshed_recently);

    std::string expected_response;
    if (fail_fetch_policies) {
      expected_response = "Invalid json response";
    } else {
      UserPolicies policies;
      base::Value policies_value = policies.ToValue();
      auto expected_response_value =
          base::Value::Dict().Set("policies", std::move(policies_value));
      base::JSONWriter::Write(expected_response_value, &expected_response);
    }

    fake_http_url_fetcher_factory()->SetFakeResponse(
        UserPoliciesManager::Get()->GetGcpwServiceUserPoliciesUrl(sid),
        FakeWinHttpUrlFetcher::Headers(), expected_response);
  }

  // Change token response to an valid one.
  SetDefaultTokenHandleResponse(kDefaultValidTokenHandleResponse);

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Finish logon successfully.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

  base::TimeDelta time_since_last_fetch =
      GetTimeDeltaSinceLastFetch(sid, kLastUserPolicyRefreshTimeRegKey);

  if (cloud_policies_enabled && !policy_refreshed_recently) {
    ASSERT_EQ(1, fake_user_policies_manager.GetNumTimesFetchAndStoreCalled());
  } else {
    ASSERT_EQ(0, fake_user_policies_manager.GetNumTimesFetchAndStoreCalled());
  }

  // Expected number of HTTP calls when not fetching user policies since upload
  // device details is always called.
  const size_t base_num_http_requests = 1;
  const size_t requests_created =
      fake_http_url_fetcher_factory()->requests_created();
  if (!cloud_policies_enabled || policy_refreshed_recently) {
    // No new requests for fetching policies.
    ASSERT_EQ(base_num_http_requests, requests_created);
  } else {
    // Verify the fetch status matches expected value.
    HRESULT hr = UserPoliciesManager::Get()->GetLastFetchStatusForTesting();

    if (!fail_fetch_policies) {
      ASSERT_TRUE(SUCCEEDED(hr));
      // One additional request for fetching policies.
      ASSERT_EQ(1 + base_num_http_requests, requests_created);
      ASSERT_TRUE(time_since_last_fetch <
                  kMaxTimeDeltaSinceLastUserPolicyRefresh);
    } else {
      ASSERT_TRUE(FAILED(hr));
      // Two additional requests since we retry on failure.
      ASSERT_EQ(2 + base_num_http_requests, requests_created);
      ASSERT_TRUE(time_since_last_fetch >
                  kMaxTimeDeltaSinceLastUserPolicyRefresh);
    }
  }

  ASSERT_EQ(S_OK, ReleaseProvider());
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpGaiaCredentialBaseFetchCloudPoliciesTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Test that correct Omaha update tracks are set when auto update policies are
// defined.
// Parameters are:
// 1. bool   : Whether cloud policies feature is enabled.
// 2. bool   : Whether GCPW auto update policy is enabled.
// 3. string : GCPW pinned version policy set through the cloud policy.
// 4. string : Existing update channel (Ex. 'beta') specified in the Omaha
//             update track registry entry for GCPW application. Empty value
//             is stable channel.
// 5. string : Current value of GCPW pinned version.
class GcpGaiaCredentialBaseOmahaUpdatePolicyTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<std::tuple<bool,
                                                      bool,
                                                      const wchar_t*,
                                                      const wchar_t*,
                                                      const wchar_t*>> {};

TEST_P(GcpGaiaCredentialBaseOmahaUpdatePolicyTest, EnforceUpdatePolicy) {
  bool cloud_policies_enabled = std::get<0>(GetParam());
  bool enable_gcpw_auto_update = std::get<1>(GetParam());
  std::wstring gcpw_pinned_version(std::get<2>(GetParam()));
  std::wstring update_channel(std::get<3>(GetParam()));
  std::wstring current_pinned_version(std::get<4>(GetParam()));

  FakeDevicePoliciesManager fake_device_policies_manager(
      cloud_policies_enabled);

  DevicePolicies device_policies;
  device_policies.enable_gcpw_auto_update = enable_gcpw_auto_update;
  device_policies.gcpw_pinned_version =
      GcpwVersion(base::WideToUTF8(gcpw_pinned_version));
  fake_device_policies_manager.SetDevicePolicies(device_policies);

  const std::wstring current_gcpw_version(L"80.1.422.2");

  // Add expected Omaha registry paths
  base::win::RegKey clientsKey, clientsStateKey;
  EXPECT_EQ(ERROR_SUCCESS,
            clientsKey.Create(HKEY_LOCAL_MACHINE, kRegUpdaterClientsAppPath,
                              KEY_SET_VALUE | KEY_WOW64_32KEY));
  EXPECT_EQ(ERROR_SUCCESS, clientsKey.WriteValue(kRegVersionName,
                                                 current_gcpw_version.c_str()));
  EXPECT_EQ(
      ERROR_SUCCESS,
      clientsStateKey.Create(HKEY_LOCAL_MACHINE, kRegUpdaterClientStateAppPath,
                             KEY_SET_VALUE | KEY_WOW64_32KEY));

  // Set existing update tracks including the currently pinned version.
  std::wstring current_update_track = current_pinned_version;
  if (!update_channel.empty())
    current_update_track = update_channel + L"-" + current_pinned_version;

  if (!current_update_track.empty()) {
    EXPECT_EQ(ERROR_SUCCESS,
              clientsStateKey.WriteValue(kRegUpdateTracksName,
                                         current_update_track.c_str()));
  }

  // Create a fake user associated to a gaia id.
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      kDefaultUsername, L"password", L"Full Name", L"comment",
                      base::UTF8ToWide(kDefaultGaiaId), std::wstring(), &sid));

  // Change token response to an valid one.
  SetDefaultTokenHandleResponse(kDefaultValidTokenHandleResponse);

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Finish logon successfully.
  ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

  ASSERT_EQ(S_OK, ReleaseProvider());

  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, kRegUpdaterClientStateAppPath,
                     KEY_READ | KEY_WOW64_32KEY));

  std::wstring update_track_value;
  LONG status = key.ReadValue(kRegUpdateTracksName, &update_track_value);

  if (cloud_policies_enabled) {
    if (device_policies.enable_gcpw_auto_update) {
      if (device_policies.gcpw_pinned_version.IsValid()) {
        // Check if pinned version is set.
        ASSERT_EQ(ERROR_SUCCESS, status);
        std::wstring expected_ap_value = gcpw_pinned_version;
        if (!update_channel.empty())
          expected_ap_value = update_channel + L"-" + gcpw_pinned_version;
        ASSERT_EQ(expected_ap_value, update_track_value);
      } else {
        // Update track should be reset to the channel it was on before.
        if (update_channel.empty()) {
          ASSERT_NE(ERROR_SUCCESS, status);
        } else {
          ASSERT_EQ(ERROR_SUCCESS, status);
          ASSERT_EQ(update_channel, update_track_value);
        }
      }
    } else {
      // Auto update is turned off.
      ASSERT_EQ(ERROR_SUCCESS, status);
      std::wstring expected_ap_value = current_gcpw_version;
      if (!update_channel.empty())
        expected_ap_value = update_channel + L"-" + current_gcpw_version;
      ASSERT_EQ(expected_ap_value, update_track_value);
    }
  } else {
    // There should be no change to existing update tracks.
    if (current_update_track.empty()) {
      ASSERT_NE(ERROR_SUCCESS, status);
    } else {
      ASSERT_EQ(ERROR_SUCCESS, status);
      ASSERT_EQ(current_update_track, update_track_value);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpGaiaCredentialBaseOmahaUpdatePolicyTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Bool(),
                       ::testing::Values(L"", L"81.1.33.42"),
                       ::testing::Values(L"", L"beta"),
                       ::testing::Values(L"", L"80.2.35.4")));

// Test the allowed domains to login policy defined either through registry or
// through a cloud policy.
// Parameters are:
// 1. int  : 0 - Domains set through cloud policy.
//           1 - Domains set through deprecated "ed" registry entry.
//           2 - Domains set through "domains_allowed_to_login" registry entry.
// 2. string : List of domains from which users are allowed to login.
class GcpGaiaCredentialBaseAllowedDomainsCloudPolicyTest
    : public GcpGaiaCredentialBaseTest,
      public ::testing::WithParamInterface<std::tuple<int, const wchar_t*>> {
 public:
  void SetUp() override;
};

void GcpGaiaCredentialBaseAllowedDomainsCloudPolicyTest::SetUp() {
  GcpGaiaCredentialBaseTest::SetUp();

  // Delete any existing registry entries. Setting to empty deletes them.
  SetGlobalFlagForTesting(L"ed", L"");
  SetGlobalFlagForTesting(L"domains_allowed_to_login", L"");
}

TEST_P(GcpGaiaCredentialBaseAllowedDomainsCloudPolicyTest, OmahaPolicyTest) {
  bool cloud_policies_enabled = std::get<0>(GetParam()) == 0;
  bool use_old_domains_reg_key = std::get<0>(GetParam()) == 1;
  std::wstring allowed_domains(std::get<1>(GetParam()));

  FakeDevicePoliciesManager fake_device_policies_manager(
      cloud_policies_enabled);

  if (cloud_policies_enabled) {
    DevicePolicies device_policies;
    if (!allowed_domains.empty()) {
      device_policies.domains_allowed_to_login = base::SplitString(
          allowed_domains, L",", base::WhitespaceHandling::TRIM_WHITESPACE,
          base::SplitResult::SPLIT_WANT_NONEMPTY);
    }
    fake_device_policies_manager.SetDevicePolicies(device_policies);
  } else if (use_old_domains_reg_key) {
    SetGlobalFlagForTesting(L"ed", allowed_domains);
  } else {
    SetGlobalFlagForTesting(L"domains_allowed_to_login", allowed_domains);
  }

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));
  ASSERT_EQ(S_OK, test->SetGlsEmailAddress("roadrunner@acme.com"));

  if (allowed_domains.empty()) {
    ASSERT_EQ(S_OK,
              StartLogonProcess(/*succeeds=*/false, IDS_EMAIL_MISMATCH_BASE));
  } else {
    ASSERT_EQ(S_OK, StartLogonProcessAndWait());

    // Finish logon successfully.
    ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));

    ASSERT_EQ(S_OK, ReleaseProvider());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpGaiaCredentialBaseAllowedDomainsCloudPolicyTest,
    ::testing::Combine(::testing::Values(0, 1, 2),
                       ::testing::Values(L"", L"acme.com,acme.org")));

}  // namespace testing
}  // namespace credential_provider
