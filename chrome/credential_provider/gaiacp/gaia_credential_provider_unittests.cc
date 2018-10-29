// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>
#include <credentialprovider.h>

#include "base/synchronization/waitable_event.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

class GcpCredentialProviderTest : public ::testing::Test {
 public:
  void CreateGCPWUser(const wchar_t* username,
                      const wchar_t* password,
                      const wchar_t* fullname,
                      const wchar_t* comment,
                      BSTR* sid) {
    DWORD error;
    ASSERT_EQ(S_OK, fake_os_user_manager_.AddUser(username, password, fullname,
                                                  comment, true, sid, &error));
    ASSERT_EQ(S_OK, SetUserProperty(OLE2CW(*sid), L"nr", 0));
    ASSERT_EQ(S_OK, SetUserProperty(OLE2CW(*sid), L"th", L"th_value"));
  }

  void CreateDeletedGCPWUser(BSTR* sid) {
    PSID sid_deleted;
    ASSERT_EQ(S_OK, fake_os_user_manager_.CreateNewSID(&sid_deleted));
    wchar_t* user_sid_string = nullptr;
    ASSERT_TRUE(ConvertSidToStringSid(sid_deleted, &user_sid_string));
    *sid = SysAllocString(W2COLE(user_sid_string));

    ASSERT_EQ(S_OK, SetUserProperty(user_sid_string, L"nr", 0));
    ASSERT_EQ(S_OK, SetUserProperty(user_sid_string, L"th", L"th_value"));
    LocalFree(user_sid_string);
  }

  FakeWinHttpUrlFetcherFactory* url_fetcher_factory() {
    return &url_fetcher_factory_;
  }

 private:
  void SetUp() override;

  registry_util::RegistryOverrideManager registry_override_;
  FakeWinHttpUrlFetcherFactory url_fetcher_factory_;
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
};

void GcpCredentialProviderTest::SetUp() {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
}

TEST_F(GcpCredentialProviderTest, Basic) {
  CComPtr<IGaiaCredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_IGaiaCredentialProvider, (void**)&provider));
}

TEST_F(GcpCredentialProviderTest, CleanupStaleTokenHandles) {
  // Simulate a user created by GCPW that does not have a stale handle.
  CComBSTR sid_good;
  CreateGCPWUser(L"username", L"password", L"Full Name", L"Comment", &sid_good);

  // Simulate a user created by GCPW that was deleted from the machine.
  CComBSTR sid_bad;
  CreateDeletedGCPWUser(&sid_bad);

  // Now create the provider.
  CComPtr<IGaiaCredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_IGaiaCredentialProvider, (void**)&provider));

  // Expect "good" sid to still in the registry, "bad" one to be cleaned up.
  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE,
                                    GetUsersRootKeyForTesting(), KEY_READ));
  EXPECT_EQ(ERROR_SUCCESS, key.OpenKey(OLE2CW(sid_good), KEY_READ));

  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE,
                                    GetUsersRootKeyForTesting(), KEY_READ));
  EXPECT_NE(ERROR_SUCCESS, key.OpenKey(OLE2CW(sid_bad), KEY_READ));
}

TEST_F(GcpCredentialProviderTest, SetUserArray) {
  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(
      S_OK,
      CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
          nullptr, IID_ICredentialProviderSetUserArray, (void**)&user_array));

  FakeCredentialProviderUserArray array;
  array.AddUser(L"sid", L"username");
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK, user_array.QueryInterface(&provider));

  // There should be no credentials.  The user added above should be ignored
  // because it does not need reauth.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  EXPECT_EQ(0u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);
}

TEST_F(GcpCredentialProviderTest, SetUserArray_NeedsReauth) {
  // Create a GCPW user user as needing reauth.
  CComBSTR sid;
  CreateGCPWUser(L"username", L"password", L"Full Name", L"Comment", &sid);
  ASSERT_EQ(S_OK, SetUserProperty(OLE2CW(sid), L"nr", 1));

  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(
      S_OK,
      CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
          nullptr, IID_ICredentialProviderSetUserArray, (void**)&user_array));

  FakeCredentialProviderUserArray array;
  array.AddUser(OLE2CW(sid), L"username");
  array.AddUser(L"sid2", L"username2");
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK, user_array.QueryInterface(&provider));

  // There should be 1 credential.  It should implement IReauthCredential.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));
  CComPtr<IReauthCredential> reauth;
  EXPECT_EQ(S_OK, cred.QueryInterface(&reauth));
}

TEST_F(GcpCredentialProviderTest, SetUserArray_PasswordChanged) {
  // Create two GCPW users that are not marked as needing reauth.
  CComBSTR sid1;
  CreateGCPWUser(L"u1", L"p1", L"n1", L"c1", &sid1);

  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(
      S_OK,
      CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
          nullptr, IID_ICredentialProviderSetUserArray, (void**)&user_array));

  base::WaitableEvent reauth_check_done_event;
  CComPtr<IGaiaCredentialProviderForTesting> for_testing;
  ASSERT_EQ(S_OK, user_array.QueryInterface(&for_testing));
  ASSERT_EQ(S_OK,
            for_testing->SetReauthCheckDoneEvent(
                reinterpret_cast<INT_PTR>(reauth_check_done_event.handle())));

  url_fetcher_factory()->SetFakeResponse(
      GURL("https://www.googleapis.com/oauth2/v2/tokeninfo"),
      FakeWinHttpUrlFetcher::Headers(), "{\"error\":\"foo\"}");

  FakeCredentialProviderUserArray array;
  array.AddUser(OLE2CW(sid1), L"u1");
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK, user_array.QueryInterface(&provider));

  // There should be no credentials since none need reauth.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(0u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);

  // After the network check, account should be marked as needing reauth.
  reauth_check_done_event.Wait();
  DWORD needs_reauth;
  ASSERT_EQ(S_OK, GetUserProperty(OLE2CW(sid1), L"nr", &needs_reauth));
  ASSERT_EQ(1u, needs_reauth);
}

TEST_F(GcpCredentialProviderTest, CpusLogon) {
  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_ICredentialProvider, (void**)&provider));

  // Start process for logon screen.
  ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_LOGON, 0));

  // Give list of users visible on welcome screen.
  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(S_OK, provider.QueryInterface(&user_array));
  FakeCredentialProviderUserArray array;
  array.AddUser(L"sid1", L"username1");
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  // Activate the CP.
  FakeCredentialProviderEvents events;
  ASSERT_EQ(S_OK, provider->Advise(&events, 0));

  // Check credentials.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));
  CComPtr<IGaiaCredential> gaia_cred;
  EXPECT_EQ(S_OK, cred.QueryInterface(&gaia_cred));

  // Get fields.
  DWORD field_count;
  ASSERT_EQ(S_OK, provider->GetFieldDescriptorCount(&field_count));
  EXPECT_EQ(4u, field_count);

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
}

TEST_F(GcpCredentialProviderTest, CpusUnlock) {
  CComPtr<ICredentialProvider> provider;
  ASSERT_EQ(S_OK,
            CComCreator<CComObject<CGaiaCredentialProvider>>::CreateInstance(
                nullptr, IID_ICredentialProvider, (void**)&provider));

  // Start process for logon screen.
  ASSERT_EQ(S_OK, provider->SetUsageScenario(CPUS_UNLOCK_WORKSTATION, 0));

  // Give list of users visible on welcome screen.
  CComPtr<ICredentialProviderSetUserArray> user_array;
  ASSERT_EQ(S_OK, provider.QueryInterface(&user_array));
  FakeCredentialProviderUserArray array;
  array.AddUser(L"sid1", L"username1");
  ASSERT_EQ(S_OK, user_array->SetUserArray(&array));

  // Activate the CP.
  FakeCredentialProviderEvents events;
  ASSERT_EQ(S_OK, provider->Advise(&events, 0));

  // Check credentials.
  DWORD count;
  DWORD default_index;
  BOOL autologon;
  ASSERT_EQ(S_OK,
            provider->GetCredentialCount(&count, &default_index, &autologon));
  ASSERT_EQ(1u, count);
  EXPECT_EQ(CREDENTIAL_PROVIDER_NO_DEFAULT, default_index);
  EXPECT_FALSE(autologon);
  CComPtr<ICredentialProviderCredential> cred;
  ASSERT_EQ(S_OK, provider->GetCredentialAt(0, &cred));
  CComPtr<IGaiaCredential> gaia_cred;
  EXPECT_EQ(S_OK, cred.QueryInterface(&gaia_cred));

  // Get fields.
  DWORD field_count;
  ASSERT_EQ(S_OK, provider->GetFieldDescriptorCount(&field_count));
  EXPECT_EQ(4u, field_count);

  // Deactivate the CP.
  ASSERT_EQ(S_OK, provider->UnAdvise());
}

}  // namespace credential_provider
