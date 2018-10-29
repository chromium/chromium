// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>

#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

class GcpGaiaCredentialTest : public ::testing::Test {
 private:
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
};

TEST_F(GcpGaiaCredentialTest, FinishAuthentication) {
  USES_CONVERSION;
  CComPtr<IGaiaCredential> cred;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CGaiaCredential>>::CreateInstance(
                      nullptr, IID_IGaiaCredential, (void**)&cred));

  CComBSTR sid;
  CComBSTR error;
  ASSERT_EQ(S_OK, cred->FinishAuthentication(CComBSTR(W2COLE(L"username")),
                                             CComBSTR(W2COLE(L"password")),
                                             CComBSTR(W2COLE(L"Full Name")),
                                             &sid, &error));
}

TEST_F(GcpGaiaCredentialTest, FinishAuthentication_SamePassword) {
  USES_CONVERSION;
  CComPtr<IGaiaCredential> cred;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CGaiaCredential>>::CreateInstance(
                      nullptr, IID_IGaiaCredential, (void**)&cred));

  CComBSTR sid;
  CComBSTR error;
  ASSERT_EQ(S_OK, cred->FinishAuthentication(CComBSTR(W2COLE(L"username")),
                                             CComBSTR(W2COLE(L"password")),
                                             CComBSTR(W2COLE(L"Full Name")),
                                             &sid, &error));

  // Finishing with the same username+password should succeeded.
  CComBSTR sid2;
  CComBSTR error2;
  ASSERT_EQ(S_OK, cred->FinishAuthentication(CComBSTR(W2COLE(L"username")),
                                             CComBSTR(W2COLE(L"password")),
                                             CComBSTR(W2COLE(L"Full Name")),
                                             &sid2, &error2));
  ASSERT_EQ(sid, sid2);
}

TEST_F(GcpGaiaCredentialTest, FinishAuthentication_DiffPassword) {
  USES_CONVERSION;
  CComPtr<IGaiaCredential> cred;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CGaiaCredential>>::CreateInstance(
                      nullptr, IID_IGaiaCredential, (void**)&cred));

  CComBSTR sid;
  CComBSTR error;
  ASSERT_EQ(S_OK, cred->FinishAuthentication(CComBSTR(W2COLE(L"username")),
                                             CComBSTR(W2COLE(L"password")),
                                             CComBSTR(W2COLE(L"Full Name")),
                                             &sid, &error));

  // Finishing with the same username but different password should fail.
  ASSERT_EQ(HRESULT_FROM_WIN32(NERR_UserExists),
            cred->FinishAuthentication(
                CComBSTR(W2COLE(L"username")), CComBSTR(W2COLE(L"password2")),
                CComBSTR(W2COLE(L"Full Name")), &sid, &error));
}

}  // namespace credential_provider
