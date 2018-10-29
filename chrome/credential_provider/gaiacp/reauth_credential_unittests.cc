// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>

#include "base/test/test_reg_util_win.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/reauth_credential.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

class GcpReauthCredentialTest : public ::testing::Test {
 private:
  void SetUp() override;

  registry_util::RegistryOverrideManager registry_override_;
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
};

void GcpReauthCredentialTest::SetUp() {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
}

TEST_F(GcpReauthCredentialTest, SetUserInfo) {
  CComPtr<IReauthCredential> reauth;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CReauthCredential>>::CreateInstance(
                      nullptr, IID_IReauthCredential, (void**)&reauth));
  ASSERT_TRUE(!!reauth);

  const CComBSTR kSid(W2COLE(L"sid"));
  ASSERT_EQ(S_OK, reauth->SetUserInfo(kSid, CComBSTR(W2COLE(L"email"))));

  CComPtr<ICredentialProviderCredential2> cpc2;
  ASSERT_EQ(S_OK, reauth->QueryInterface(IID_ICredentialProviderCredential2,
                                         reinterpret_cast<void**>(&cpc2)));

  wchar_t* sid;
  ASSERT_EQ(S_OK, cpc2->GetUserSid(&sid));
  ASSERT_EQ(kSid, CComBSTR(W2COLE(sid)));
  ::CoTaskMemFree(sid);
}

TEST_F(GcpReauthCredentialTest, FinishAuthentication) {
  // Create a fake user to reauth.
  const wchar_t* kUsername = L"username";
  OSUserManager* manager = OSUserManager::Get();
  CComBSTR sid;
  DWORD error;
  ASSERT_EQ(S_OK, manager->AddUser(kUsername, L"password", L"fullname",
                                   L"comment", true, &sid, &error));

  // Initialize a reauth credential.
  CComPtr<IReauthCredential> reauth;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CReauthCredential>>::CreateInstance(
                      nullptr, IID_IReauthCredential, (void**)&reauth));
  const CComBSTR kSid(W2COLE(L"sid"));
  ASSERT_EQ(S_OK, reauth->SetUserInfo(kSid, CComBSTR(W2COLE(L"email"))));

  CComPtr<IGaiaCredential> cred;
  ASSERT_EQ(S_OK, reauth->QueryInterface(IID_IGaiaCredential,
                                         reinterpret_cast<void**>(&cred)));

  // Finishing reauth with an invalid username should fail.
  CComBSTR error2;
  ASSERT_NE(S_OK, cred->FinishAuthentication(
                      CComBSTR(W2COLE(L"user2")), CComBSTR(W2COLE(L"password")),
                      CComBSTR(W2COLE(L"Full Name")), &sid, &error2));
  sid.Empty();
  error2.Empty();

  // Finishing reauth with an correct username should succeed.
  ASSERT_EQ(S_OK, cred->FinishAuthentication(CComBSTR(W2COLE(kUsername)),
                                             CComBSTR(W2COLE(L"password")),
                                             CComBSTR(W2COLE(L"Full Name")),
                                             &sid, &error2));
}

TEST_F(GcpReauthCredentialTest, OnUserAuthenticated) {
  // Create a fake credential provider.  This object must outlive the reauth
  // credential so it should be declared first.
  FakeGaiaCredentialProvider provider;

  // Initialize a reauth credential.
  CComPtr<IReauthCredential> reauth;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CReauthCredential>>::CreateInstance(
                      nullptr, IID_IReauthCredential, (void**)&reauth));
  const CComBSTR kSid(W2COLE(L"sid"));
  ASSERT_EQ(S_OK, reauth->SetUserInfo(kSid, CComBSTR(W2COLE(L"email"))));

  CComPtr<IGaiaCredential> cred;
  ASSERT_EQ(S_OK, reauth->QueryInterface(IID_IGaiaCredential,
                                         reinterpret_cast<void**>(&cred)));
  ASSERT_EQ(S_OK, cred->Initialize(&provider));

  // Finishing reauth with an correct username should succeed.
  CComBSTR kUsername(W2COLE(L"username"));
  CComBSTR kPassword(W2COLE(L"password"));
  ASSERT_EQ(S_OK, cred->OnUserAuthenticated(kUsername, kPassword, kSid));

  // Check that values were propagated to the provider.
  EXPECT_EQ(kUsername, provider.username());
  EXPECT_EQ(kPassword, provider.password());
  EXPECT_EQ(kSid, provider.sid());

  ASSERT_EQ(S_OK, cred->Terminate());
}

}  // namespace credential_provider
