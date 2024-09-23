// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"

#include <credentialprovider.h>
#include <wrl/client.h>

#include <memory>
#include <tuple>

#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/atl.h"
#include "base/win/win_util.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/auth_utils.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/credential_provider/test/test_credential.h"

namespace credential_provider {

namespace testing {

class GcpCredentialProviderTest : public GlsRunnerTestBase {
 protected:
  void CreateDefaultCloudPoliciesForUser(const std::wstring& sid);
  void SetCloudPoliciesForUser(const std::wstring& sid,
                               const UserPolicies policies);

 private:
  std::unique_ptr<FakeUserPoliciesManager> fake_user_policies_manager_;
  std::unique_ptr<FakeTokenGenerator> fake_token_generator_;
};

void GcpCredentialProviderTest::CreateDefaultCloudPoliciesForUser(
    const std::wstring& sid) {
  UserPolicies policies;
  SetCloudPoliciesForUser(sid, policies);
}

void GcpCredentialProviderTest::SetCloudPoliciesForUser(
    const std::wstring& sid,
    const UserPolicies policies) {
  if (!fake_user_policies_manager_)
    fake_user_policies_manager_ = std::make_unique<FakeUserPoliciesManager>();
  if (!fake_token_generator_)
    fake_token_generator_ = std::make_unique<FakeTokenGenerator>();

  // Ensure user has policies and valid GCPW token.
  fake_user_policies_manager_->SetUserPolicies(sid, policies);
  fake_user_policies_manager_->SetUserPolicyStaleOrMissing(sid, false);

  std::string dm_token = "test-gcpw-dm-token";
  fake_token_generator_->SetTokensForTesting({dm_token});
  EXPECT_EQ(S_OK, GenerateGCPWDmToken(sid));
}

TEST_F(GcpCredentialProviderTest, Basic) {
  Microsoft::WRL::ComPtr<IGaiaCredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_PPV_ARGS(&provider)));
}

TEST_F(GcpCredentialProviderTest, SetUserArray_NoGaiaUsers) {
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"full name", L"comment", L"",
                      L"", &sid));

  Microsoft::WRL::ComPtr<ICredentialProvider> provider;
  DWORD count = 0;
  ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

  // There should only be the anonymous credential. Only users with the
  // requisite registry entry will be counted.
  EXPECT_EQ(1u, count);

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));

  Microsoft::WRL::ComPtr<ICredentialProviderCredential2> cred2;
  ASSERT_NE(S_OK, cred.As(&cred2));

  Microsoft::WRL::ComPtr<IReauthCredential> reauth_cred;
  ASSERT_NE(S_OK, cred.As(&reauth_cred));
}

TEST_F(GcpCredentialProviderTest, CpusLogon) {
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"full name", L"comment", L"",
                      L"", &sid));

  Microsoft::WRL::ComPtr<ICredentialProvider> provider;
  DWORD count = 0;
  ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

  // There should only be the anonymous credential. Only users with the
  // requisite registry entry will be counted.
  EXPECT_EQ(1u, count);

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));

  Microsoft::WRL::ComPtr<ICredentialProviderCredential2> cred2;
  ASSERT_NE(S_OK, cred.As(&cred2));

  Microsoft::WRL::ComPtr<IReauthCredential> reauth_cred;
  ASSERT_NE(S_OK, cred.As(&reauth_cred));
}

TEST_F(GcpCredentialProviderTest, CpusUnlock) {
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"full name", L"comment", L"",
                      L"", &sid));

  Microsoft::WRL::ComPtr<ICredentialProvider> provider;
  DWORD count = 0;
  SetUsageScenario(CPUS_UNLOCK_WORKSTATION);
  ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

  // Check credentials. None should be available because the anonymous
  // credential is not allowed during an unlock scenario.
  ASSERT_EQ(0u, count);
}

TEST_F(GcpCredentialProviderTest, AutoLogonAfterUserRefresh) {
  USES_CONVERSION;
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"full name", L"comment", L"",
                      L"", &sid));

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ICredentialProvider> provider = created_provider();

  Microsoft::WRL::ComPtr<IGaiaCredentialProvider> gaia_provider;
  ASSERT_EQ(S_OK, provider.As(&gaia_provider));

  // Notify that user access is denied to fake a forced recreation of the users.
  Microsoft::WRL::ComPtr<ICredentialUpdateEventsHandler> update_handler;
  ASSERT_EQ(S_OK, provider.As(&update_handler));
  update_handler->UpdateCredentialsIfNeeded(true);

  // Credential changed event should have been received.
  EXPECT_TRUE(fake_provider_events()->CredentialsChangedReceived());
  fake_provider_events()->ResetCredentialsChangedReceived();

  // At the same time notify that a user has authenticated and requires a
  // sign in.
  {
    // Temporary locker to prevent DCHECKs in OnUserAuthenticated
    AssociatedUserValidator::ScopedBlockDenyAccessUpdate deny_update_locker(
        AssociatedUserValidator::Get());
    ASSERT_EQ(S_OK, gaia_provider->OnUserAuthenticated(
                        cred.Get(), CComBSTR(L"username"),
                        CComBSTR(L"password"), sid, true));
  }

  // No credential changed should have been signalled here.
  EXPECT_FALSE(fake_provider_events()->CredentialsChangedReceived());

  // GetCredentialCount should return back the same credential that was just
  // auto logged on.

  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(0u, default_index);
  EXPECT_TRUE(autologon);

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> auto_logon_cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &auto_logon_cred));
  EXPECT_EQ(auto_logon_cred, cred);

  // The next call to GetCredentialCount should return re-created credentials.

  // Fake an update request with no access changes. The pending user refresh
  // should be queued.
  update_handler->UpdateCredentialsIfNeeded(false);

  // Credential changed event should have been received.
  EXPECT_TRUE(fake_provider_events()->CredentialsChangedReceived());

  // GetCredentialCount should return new credentials with no auto logon.
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> new_cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &new_cred));
  EXPECT_NE(new_cred, cred);

  // Another request to refresh the credentials should yield no credential
  // changed event or refresh of credentials.
  fake_provider_events()->ResetCredentialsChangedReceived();

  update_handler->UpdateCredentialsIfNeeded(false);

  // No credential changed event should have been received.
  EXPECT_FALSE(fake_provider_events()->CredentialsChangedReceived());

  // GetCredentialCount should return the same credentials with no change.
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> unchanged_cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &unchanged_cred));
  EXPECT_EQ(new_cred, unchanged_cred);
}

TEST_F(GcpCredentialProviderTest, AutoLogonBeforeUserRefresh) {
  USES_CONVERSION;
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"username", L"password", L"full name", L"comment", L"",
                      L"", &sid));

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ICredentialProvider> provider = created_provider();
  Microsoft::WRL::ComPtr<IGaiaCredentialProvider> gaia_provider;
  ASSERT_EQ(S_OK, provider.As(&gaia_provider));

  Microsoft::WRL::ComPtr<ICredentialUpdateEventsHandler> update_handler;
  ASSERT_EQ(S_OK, provider.As(&update_handler));

  // Notify user auto logon first and then notify user access denied to ensure
  // that auto logon always has precedence over user access denied.
  {
    // Temporary locker to prevent DCHECKs in OnUserAuthenticated
    AssociatedUserValidator::ScopedBlockDenyAccessUpdate deny_update_locker(
        AssociatedUserValidator::Get());
    ASSERT_EQ(S_OK, gaia_provider->OnUserAuthenticated(
                        cred.Get(), CComBSTR(L"username"),
                        CComBSTR(L"password"), sid, true));
  }

  // Credential changed event should have been received.
  EXPECT_TRUE(fake_provider_events()->CredentialsChangedReceived());
  fake_provider_events()->ResetCredentialsChangedReceived();

  // Notify that user access is denied. This should not cause a credential
  // changed since an event was already processed.
  update_handler->UpdateCredentialsIfNeeded(true);

  // No credential changed should have been signalled here.
  EXPECT_FALSE(fake_provider_events()->CredentialsChangedReceived());

  // GetCredentialCount should return back the same credential that was just
  // auto logged on.
  DWORD count;
  DWORD default_index;
  BOOL autologon;

  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(0u, default_index);
  EXPECT_TRUE(autologon);

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> auto_logon_cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &auto_logon_cred));
  EXPECT_EQ(auto_logon_cred, cred);

  // The next call to GetCredentialCount should return re-created credentials.

  // Fake an update request with no access changes. The pending user refresh
  // should be queued.
  update_handler->UpdateCredentialsIfNeeded(false);

  // Credential changed event should have been received.
  EXPECT_TRUE(fake_provider_events()->CredentialsChangedReceived());

  // GetCredentialCount should return new credentials with no auto logon.
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> new_cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &new_cred));
  EXPECT_NE(new_cred, cred);

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
}

TEST_F(GcpCredentialProviderTest, AddPersonAfterUserRemove) {
  GoogleUploadDeviceDetailsNeededForTesting upload_device_details_needed(false);

  // Set up such that multi-users is not enabled, and a user already
  // exists.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));

  const wchar_t kDummyUsername[] = L"username";
  const wchar_t kDummyPassword[] = L"password";
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      kDummyUsername, kDummyPassword, L"full name", L"comment",
                      L"gaia-id", L"foo@gmail.com", &sid));

  CreateDefaultCloudPoliciesForUser((BSTR)sid);

  {
    Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
    Microsoft::WRL::ComPtr<ICredentialProvider> provider;
    DWORD count = 0;
    ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

    // In this case no credential should be returned.
    ASSERT_EQ(0u, count);

    // Release the CP so we can create another one.
    ASSERT_EQ(S_OK, ReleaseProvider());
  }

  // Delete the OS user.  At this point, info in the HKLM registry about this
  // user will still exist.  However it gets properly cleaned up when the token
  // validator starts its refresh of token handle validity.
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->RemoveUser(kDummyUsername, kDummyPassword));

  {
    Microsoft::WRL::ComPtr<ICredentialProvider> provider;
    DWORD count = 0;
    ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

    // This time a credential should be returned.
    ASSERT_EQ(1u, count);

    // And this credential should be the anonymous one.
    Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
    ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));

    Microsoft::WRL::ComPtr<ICredentialProviderCredential2> cred2;
    ASSERT_NE(S_OK, cred.As(&cred2));

    Microsoft::WRL::ComPtr<IReauthCredential> reauth_cred;
    ASSERT_NE(S_OK, cred.As(&reauth_cred));

    // Release the CP.
    ASSERT_EQ(S_OK, provider->UnAdvise());
  }
}

class GcpCredentialProviderExecutionTest : public GlsRunnerTestBase {};

TEST_F(GcpCredentialProviderExecutionTest, UnAdviseDuringGls) {
  USES_CONVERSION;

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  // This event is merely used to keep the gls running while it is killed by
  // Terminate().
  constexpr wchar_t kStartGlsEventName[] = L"UnAdviseDuringGls_Signal";
  base::win::ScopedHandle start_event_handle(
      ::CreateEvent(nullptr, false, false, kStartGlsEventName));
  ASSERT_TRUE(start_event_handle.IsValid());
  ASSERT_EQ(S_OK, test->SetStartGlsEventName(kStartGlsEventName));
  base::WaitableEvent start_event(std::move(start_event_handle));

  ASSERT_EQ(S_OK, StartLogonProcess(/*succeeds=*/true));

  // Release the provider which should also Terminate the credential that
  // was created.
  ReleaseProvider();
}

// Tests auto logon enabled when set serialization is called.
// Parameters:
// 1. bool: are the users' token handles still valid.
// 2. CREDENTIAL_PROVIDER_USAGE_SCENARIO - the usage scenario.
class GcpCredentialProviderSetSerializationTest
    : public GcpCredentialProviderTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, CREDENTIAL_PROVIDER_USAGE_SCENARIO>> {};

TEST_P(GcpCredentialProviderSetSerializationTest, CheckAutoLogon) {
  const bool valid_token_handles = std::get<0>(GetParam());
  const CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus = std::get<1>(GetParam());

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));
  GoogleUploadDeviceDetailsNeededForTesting upload_device_details_needed(false);

  CComBSTR first_sid;
  constexpr wchar_t first_username[] = L"username";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      first_username, L"password", L"full name", L"comment",
                      L"gaia-id", L"foo@gmail.com", &first_sid));
  CreateDefaultCloudPoliciesForUser((BSTR)first_sid);

  CComBSTR second_sid;
  constexpr wchar_t second_username[] = L"username2";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      second_username, L"password", L"Full Name", L"Comment",
                      L"gaia-id2", L"foo2@gmail.com", &second_sid));
  CreateDefaultCloudPoliciesForUser((BSTR)second_sid);

  // Build a dummy authentication buffer that can be passed to SetSerialization.
  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION cpcs;
  std::wstring local_domain = OSUserManager::GetLocalDomain();
  std::wstring serialization_username = second_username;
  std::wstring serialization_password = L"password";
  std::vector<wchar_t> dummy_domain(
      local_domain.c_str(), local_domain.c_str() + local_domain.size() + 1);
  std::vector<wchar_t> dummy_username(
      serialization_username.c_str(),
      serialization_username.c_str() + serialization_username.size() + 1);
  std::vector<wchar_t> dummy_password(
      serialization_password.c_str(),
      serialization_password.c_str() + serialization_password.size() + 1);
  ASSERT_EQ(S_OK, BuildCredPackAuthenticationBuffer(
                      &dummy_domain[0], &dummy_username[0], &dummy_password[0],
                      cpus, &cpcs));

  GetAuthenticationPackageId(&cpcs.ulAuthenticationPackage);
  cpcs.clsidCredentialProvider = CLSID_GaiaCredentialProvider;

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
  Microsoft::WRL::ComPtr<ICredentialProvider> provider;
  SetDefaultTokenHandleResponse(valid_token_handles
                                    ? kDefaultValidTokenHandleResponse
                                    : kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderWithRemoteCredentials(&cpcs, &provider));

  ::CoTaskMemFree(cpcs.rgbSerialization);

  // Check the correct number of credentials are created and whether autologon
  // is enabled based on the token handle validity.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));

  bool should_autologon = !valid_token_handles;
  EXPECT_EQ(valid_token_handles ? 0u : 2u, count);
  EXPECT_EQ(autologon, should_autologon);
  EXPECT_EQ(default_index,
            should_autologon ? 1 : CREDENTIAL_PROVIDER_NO_DEFAULT);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpCredentialProviderSetSerializationTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(CPUS_UNLOCK_WORKSTATION, CPUS_LOGON)));

// Check that reauth credentials only exist when the token handle for the
// associated user is no longer valid and internet is available.
// Parameters are:
// 1. bool - does the fake user have a token handle set.
// 2. bool - is the token handle for the fake user valid (i.e. the fetch of
// the token handle info from win_http_url_fetcher returns a valid json).
// 3. bool - is internet available.
// 4. bool - is active directory user.
// 5. bool - is internet not available but validity expired.
// 6. int - 0. Both GaiaID and Email are available.
//          1. Gaia ID is not available
//          2. Email is not available
//          3. Both are unavailable.
// 7. int - number of times device details upload failed.
class GcpCredentialProviderWithGaiaUsersTest
    : public GcpCredentialProviderTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, bool, bool, bool, bool, int, int>> {
 protected:
  void SetUp() override;
};

void GcpCredentialProviderWithGaiaUsersTest::SetUp() {
  GcpCredentialProviderTest::SetUp();
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(L"enable_cloud_association", 0));
}

TEST_P(GcpCredentialProviderWithGaiaUsersTest, ReauthCredentialTest) {
  const bool has_token_handle = std::get<0>(GetParam());
  const bool valid_token_handle = std::get<1>(GetParam());
  const bool has_internet = std::get<2>(GetParam());
  const bool is_ad_user = std::get<3>(GetParam());
  fake_internet_checker()->SetHasInternetConnection(
      has_internet ? FakeInternetAvailabilityChecker::kHicForceYes
                   : FakeInternetAvailabilityChecker::kHicForceNo);
  const bool is_offline_validity_expired = std::get<4>(GetParam());
  const int user_property_status = std::get<5>(GetParam());
  const int num_upload_device_details_failures = std::get<6>(GetParam());
  const bool is_upload_device_details_failed =
      num_upload_device_details_failures > 0;

  CComBSTR sid;
  if (is_ad_user) {
    // Add an AD user. Note that this covers the scenario where
    // enable_cloud_association is set to false.
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        L"username", L"password", L"full name", L"comment",
                        L"gaia-id", L"foo@gmail.com", L"domain", &sid));

  } else {
    // Add a local user.
    ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                        L"username", L"password", L"full name", L"comment",
                        L"gaia-id", L"foo@gmail.com", &sid));
  }

  if (user_property_status & 1) {
    // Gaia id is not available.
    SetUserProperty((BSTR)sid, kUserId, L"");
  }
  if (user_property_status & 2) {
    // Email is not available.
    SetUserProperty((BSTR)sid, kUserEmail, L"");
  }

  ASSERT_EQ(S_OK, SetUserProperty(OLE2CW(sid),
                                  base::UTF8ToWide(kKeyLastTokenValid), L"0"));

  UserPolicies policies;
  if (is_offline_validity_expired) {
    // Setting validity period to zero enforces gcpw login irrespective of
    // whether internet is available or not.
    policies.validity_period_days = 0;
  }

  // Ensure user has policies and valid GCPW token.
  SetCloudPoliciesForUser((BSTR)sid, policies);

  if (!has_token_handle)
    ASSERT_EQ(S_OK, SetUserProperty((BSTR)sid, kUserTokenHandle, L""));

  // Set upload device details status and failure count.
  ASSERT_EQ(S_OK, SetUserProperty((BSTR)sid, kRegDeviceDetailsUploadStatus,
                                  is_upload_device_details_failed ? 0 : 1));
  ASSERT_EQ(S_OK, SetUserProperty((BSTR)sid, kRegDeviceDetailsUploadFailures,
                                  num_upload_device_details_failures));

  Microsoft::WRL::ComPtr<ICredentialProvider> provider;
  DWORD count = 0;
  SetDefaultTokenHandleResponse(valid_token_handle
                                    ? kDefaultValidTokenHandleResponse
                                    : kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

  // should_reauth_user will be false if one of the following holds:
  // - the user properties don't contain email and gaia id
  // - no internet with offline validity hasn't expired
  // - with internet and when all of the following is satisfied:
  //   - device details upload succeeded or failed more than the max number of
  //     times failure is allowed.
  //   - has token handle
  //   - token handle is valid
  // In all other cases, reauth must be added, thus should_reauth_user is set to
  // true.
  bool should_reauth_user =
      (user_property_status != 3) &&
      ((!has_internet && is_offline_validity_expired) ||
       (has_internet && ((is_upload_device_details_failed &&
                          num_upload_device_details_failures <=
                              kMaxNumConsecutiveUploadDeviceFailures) ||
                         !has_token_handle || !valid_token_handle)));

  // Check if there is a IReauthCredential depending on the state of the token
  // handle.
  ASSERT_EQ(should_reauth_user ? 2u : 1u, count);

  if (should_reauth_user) {
    Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
    ASSERT_EQ(S_OK, provider->GetCredentialAt(1, &cred));
    Microsoft::WRL::ComPtr<IReauthCredential> reauth;
    EXPECT_EQ(S_OK, cred.As(&reauth));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpCredentialProviderWithGaiaUsersTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Values(0, 1, 2, 3),
        ::testing::Range(0, 2 * kMaxNumConsecutiveUploadDeviceFailures)));

// Check that reauth credentials only exists when either user is an AD user or
// the token handle for the associated user is no longer valid when internet is
// available.
// Parameters are:
// 1. bool - has an user_id and token handle in the registry.
// 2. bool - is the token handle for the fake user valid (i.e. the fetch of
// the token handle info from win_http_url_fetcher returns a valid json).
// 3. bool - is the fake user an AD user.
// 4. bool - is internet available.
// 5. bool - is offline validity expired.
// 6. bool - is device details upload failed.
class GcpCredentialProviderWithADUsersTest
    : public GcpCredentialProviderTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, bool, bool, bool, bool, bool>> {
 protected:
  void SetUp() override;
};

void GcpCredentialProviderWithADUsersTest::SetUp() {
  GcpCredentialProviderTest::SetUp();
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(L"enable_cloud_association", 1));
}

TEST_P(GcpCredentialProviderWithADUsersTest, ReauthCredentialTest) {
  const bool has_user_id = std::get<0>(GetParam());
  const bool valid_token_handle = std::get<1>(GetParam());
  const bool is_ad_user = std::get<2>(GetParam());
  const bool has_internet = std::get<3>(GetParam());
  const bool is_offline_validity_expired = std::get<4>(GetParam());
  const bool is_upload_device_details_failed = std::get<5>(GetParam());

  fake_internet_checker()->SetHasInternetConnection(
      has_internet ? FakeInternetAvailabilityChecker::kHicForceYes
                   : FakeInternetAvailabilityChecker::kHicForceNo);

  CComBSTR sid;
  DWORD error;
  std::wstring domain;
  if (is_ad_user) {
    // Add an AD user.
    ASSERT_EQ(S_OK, fake_os_user_manager()->AddUser(
                        L"username", L"password", L"full name", L"comment",
                        true, L"domain", &sid, &error));
  } else {
    // Add a local user.
    ASSERT_EQ(S_OK, fake_os_user_manager()->AddUser(L"username", L"password",
                                                    L"full name", L"comment",
                                                    true, &sid, &error));
  }

  UserPolicies policies;
  if (has_user_id) {
    std::wstring test_user_id(L"12345");
    ASSERT_EQ(S_OK, SetUserProperty(OLE2CW(sid), kUserId, test_user_id));
    // Set token handle to a non-empty value in registry.
    ASSERT_EQ(S_OK, SetUserProperty(OLE2CW(sid), kUserTokenHandle,
                                    L"non-empty-token-handle"));
    ASSERT_EQ(S_OK,
              SetUserProperty(OLE2CW(sid), base::UTF8ToWide(kKeyLastTokenValid),
                              L"0"));
    if (is_offline_validity_expired) {
      // Setting validity period to zero enforces gcpw login irrespective of
      // whether internet is available or not.
      policies.validity_period_days = 0;
    }

    ASSERT_EQ(S_OK, SetUserProperty((BSTR)sid, kRegDeviceDetailsUploadStatus,
                                    is_upload_device_details_failed ? 0 : 1));
  }

  // Ensure user has policies and valid GCPW token.
  SetCloudPoliciesForUser((BSTR)sid, policies);

  Microsoft::WRL::ComPtr<ICredentialProvider> provider;
  DWORD count = 0;
  SetDefaultTokenHandleResponse(valid_token_handle
                                    ? kDefaultValidTokenHandleResponse
                                    : kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

  bool should_reauth_user =
      (!has_internet && is_offline_validity_expired && has_user_id) ||
      (has_internet &&
       ((!has_user_id && is_ad_user) || (has_user_id && !valid_token_handle) ||
        (has_user_id && is_upload_device_details_failed)));

  // We expect one reauth credential for AD/Local user
  // and one anonymous credential.
  ASSERT_EQ(should_reauth_user ? 2u : 1u, count);

  if (should_reauth_user) {
    Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
    ASSERT_EQ(S_OK, provider->GetCredentialAt(1, &cred));
    Microsoft::WRL::ComPtr<IReauthCredential> reauth;
    EXPECT_EQ(S_OK, cred.As(&reauth));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpCredentialProviderWithADUsersTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Check that the correct reauth credential type is created based on various
// policy settings as well as usage scenarios.
// Parameters are:
// 1. bool - are the users' token handles still valid.
// 2. CREDENTIAL_PROVIDER_USAGE_SCENARIO - the usage scenario.
// 3. bool - is the other user tile available.
// 4. bool - is machine enrolled to mdm.
// 5. bool - is the second user locking the system.
class GcpCredentialProviderAvailableCredentialsTest
    : public GcpCredentialProviderTest,
      public ::testing::WithParamInterface<
          std::tuple<bool,
                     CREDENTIAL_PROVIDER_USAGE_SCENARIO,
                     bool,
                     bool,
                     bool>> {
 protected:
  void SetUp() override;
};

void GcpCredentialProviderAvailableCredentialsTest::SetUp() {
  GcpCredentialProviderTest::SetUp();
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmUrl, L"https://mdm.com"));
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser, 0));
}

TEST_P(GcpCredentialProviderAvailableCredentialsTest, AvailableCredentials) {
  USES_CONVERSION;

  const bool valid_token_handles = std::get<0>(GetParam());
  const CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus = std::get<1>(GetParam());
  const bool other_user_tile_available = std::get<2>(GetParam());
  const bool enrolled_to_mdm = std::get<3>(GetParam());
  const bool second_user_locking_system = std::get<4>(GetParam());

  GoogleMdmEnrolledStatusForTesting forced_status(enrolled_to_mdm);
  GoogleUploadDeviceDetailsNeededForTesting upload_device_details_needed(false);

  if (other_user_tile_available)
    fake_user_array()->SetAccountOptions(CPAO_EMPTY_LOCAL);

  CComBSTR first_sid;
  constexpr wchar_t first_username[] = L"username";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      first_username, L"password", L"full name", L"comment",
                      L"gaia-id", L"foo@gmail.com", &first_sid));
  CreateDefaultCloudPoliciesForUser((BSTR)first_sid);

  CComBSTR second_sid;
  constexpr wchar_t second_username[] = L"username2";
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      second_username, L"password", L"Full Name", L"Comment",
                      L"gaia-id2", L"foo2@gmail.com", &second_sid));
  CreateDefaultCloudPoliciesForUser((BSTR)second_sid);

  // Set the user locking the system.
  SetSidLockingWorkstation(second_user_locking_system ? OLE2CW(second_sid)
                                                      : OLE2CW(first_sid));

  Microsoft::WRL::ComPtr<ICredentialProvider> provider;
  DWORD count = 0;
  SetUsageScenario(cpus);
  SetDefaultTokenHandleResponse(valid_token_handles
                                    ? kDefaultValidTokenHandleResponse
                                    : kDefaultInvalidTokenHandleResponse);
  ASSERT_EQ(S_OK, InitializeProviderWithCredentials(&count, &provider));

  // Check the correct number of credentials are created.
  DWORD expected_credentials = 0;
  if (cpus != CPUS_UNLOCK_WORKSTATION) {
    expected_credentials = valid_token_handles && enrolled_to_mdm ? 0 : 2;
    if (other_user_tile_available)
      expected_credentials += 1;
  } else {
    if (other_user_tile_available) {
      expected_credentials = 1;
    } else {
      expected_credentials = valid_token_handles && enrolled_to_mdm ? 0 : 1;
    }
  }

  ASSERT_EQ(expected_credentials, count);

  // No credentials to verify.
  if (expected_credentials == 0)
    return;

  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;
  Microsoft::WRL::ComPtr<ICredentialProviderCredential2> cred2;
  Microsoft::WRL::ComPtr<IReauthCredential> reauth;

  DWORD first_non_anonymous_cred_index = 0;

  // Other user tile is shown, we should create the anonymous tile as a
  // ICredentialProviderCredential2 so that it is added to the "Other User"
  // tile.
  if (other_user_tile_available) {
    EXPECT_EQ(S_OK, provider->GetCredentialAt(first_non_anonymous_cred_index++,
                                              &cred));
    EXPECT_EQ(S_OK, cred.As(&cred2));
  }

  // Not unlocking workstation: if there are more credentials then they should
  // all the credentials should be shown as reauth credentials since all the
  // users have expired token handles (or need mdm enrollment) and need to
  // reauth.

  if (cpus != CPUS_UNLOCK_WORKSTATION) {
    if (first_non_anonymous_cred_index < expected_credentials) {
      EXPECT_EQ(S_OK, provider->GetCredentialAt(
                          first_non_anonymous_cred_index++, &cred));
      EXPECT_EQ(S_OK, cred.As(&reauth));
      EXPECT_EQ(S_OK, cred.As(&cred2));

      EXPECT_EQ(S_OK, provider->GetCredentialAt(
                          first_non_anonymous_cred_index++, &cred));
      EXPECT_EQ(S_OK, cred.As(&reauth));
      EXPECT_EQ(S_OK, cred.As(&cred2));
    }
  } else if (!other_user_tile_available) {
    // Only the user who locked the computer should be returned as a credential
    // and it should be a ICredentialProviderCredential2 with the correct sid.
    EXPECT_EQ(S_OK, provider->GetCredentialAt(first_non_anonymous_cred_index++,
                                              &cred));
    EXPECT_EQ(S_OK, cred.As(&reauth));
    EXPECT_EQ(S_OK, cred.As(&cred2));

    wchar_t* sid;
    EXPECT_EQ(S_OK, cred2->GetUserSid(&sid));
    EXPECT_EQ(second_user_locking_system ? second_sid : first_sid,
              CComBSTR(W2COLE(sid)));

    // In the case that a real CReauthCredential is created, we expect that this
    // credential will set the default credential provider for the user tile.
    auto guid_string = base::win::WStringFromGUID(CLSID_GaiaCredentialProvider);

    wchar_t guid_in_registry[64];
    ULONG length = std::size(guid_in_registry);
    EXPECT_EQ(S_OK, GetMachineRegString(kLogonUiUserTileRegKey, sid,
                                        guid_in_registry, &length));
    EXPECT_EQ(guid_string, std::wstring(guid_in_registry));
    ::CoTaskMemFree(sid);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GcpCredentialProviderAvailableCredentialsTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(CPUS_UNLOCK_WORKSTATION, CPUS_LOGON),
                       ::testing::Bool(),
                       ::testing::Bool(),
                       ::testing::Bool()));

// Test creation of new users when multi user mode is enabled/disabled through
// either registry or by cloud policy of existing user.
// Parameters are:
// 1. bool : Whether multi user mode is enabled in the registry.
// 2. bool : Whether cloud policies feature is enabled.
// 3. bool : Whether multi user policy is enabled for the existing user through
//           cloud polcies.
class GcpGaiaCredentialBaseMultiUserCloudPolicyTest
    : public GcpCredentialProviderTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 protected:
  void SetUp() override;
};

void GcpGaiaCredentialBaseMultiUserCloudPolicyTest::SetUp() {
  GcpCredentialProviderTest::SetUp();

  FakesForTesting fakes;
  fakes.fake_win_http_url_fetcher_creator =
      fake_http_url_fetcher_factory()->GetCreatorCallback();
  fakes.os_user_manager_for_testing = fake_os_user_manager();
  UserPoliciesManager::Get()->SetFakesForTesting(&fakes);
}

TEST_P(GcpGaiaCredentialBaseMultiUserCloudPolicyTest, CanCreateNewUsers) {
  USES_CONVERSION;
  bool reg_multi_user_enabled = std::get<0>(GetParam());
  bool cloud_policies_enabled = std::get<1>(GetParam());
  bool cloud_multi_user_enabled = std::get<2>(GetParam());

  GoogleMdmEnrolledStatusForTesting force_success(true);
  FakeUserPoliciesManager fake_user_policies_manager(cloud_policies_enabled);

  // Create a fake user that is already associated so when the user tries to
  // sign on and create a new user, it fails if multi user mode is disabled.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmAllowConsumerAccounts, 1));
  CComBSTR sid;
  ASSERT_EQ(S_OK, fake_os_user_manager()->CreateTestOSUser(
                      L"foo_registered", L"password", L"name", L"comment",
                      L"gaia-id-registered", std::wstring(), &sid));

  // Set multi user mode in registry.
  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(kRegMdmSupportsMultiUser,
                                          reg_multi_user_enabled ? 1 : 0));

  // Set multi user mode with cloud policy for the existing user.
  if (cloud_policies_enabled) {
    UserPolicies user_policies;
    user_policies.enable_multi_user_login = cloud_multi_user_enabled;
    fake_user_policies_manager.SetUserPolicies((BSTR)sid, user_policies);
  }

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

  if ((cloud_policies_enabled ? cloud_multi_user_enabled
                              : reg_multi_user_enabled)) {
    // Sign in should succeed for the new user.
    ASSERT_EQ(S_OK, FinishLogonProcess(true, true, 0));
  } else {
    // Sign in should fail with an error stating that no new users can be
    // created.
    ASSERT_EQ(S_OK,
              FinishLogonProcess(false, false, IDS_ADD_USER_DISALLOWED_BASE));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GcpGaiaCredentialBaseMultiUserCloudPolicyTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

}  // namespace testing

}  // namespace credential_provider
