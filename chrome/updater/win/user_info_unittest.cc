// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/user_info.h"

#include <windows.h>

#include <string>

#include "base/win/access_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

// Tests that GetProcessUser succeeds and returns a non-empty SID string.
TEST(UserInfoTest, GetProcessUserReturnsSid) {
  std::wstring sid;
  HRESULT hr = GetProcessUser(nullptr, nullptr, &sid);
  ASSERT_HRESULT_SUCCEEDED(hr);
  EXPECT_FALSE(sid.empty());
  // All SID SDDL strings start with "S-".
  EXPECT_EQ(sid.substr(0, 2), L"S-");
}

// Tests that GetProcessUser returns a non-empty name and domain.
TEST(UserInfoTest, GetProcessUserReturnsNameAndDomain) {
  std::wstring name;
  std::wstring domain;
  ASSERT_HRESULT_SUCCEEDED(GetProcessUser(&name, &domain, nullptr));
  EXPECT_FALSE(name.empty());
  EXPECT_FALSE(domain.empty());
}

// Tests that GetProcessUser populates all three output parameters.
TEST(UserInfoTest, GetProcessUserReturnsAll) {
  std::wstring name;
  std::wstring domain;
  std::wstring sid;
  ASSERT_HRESULT_SUCCEEDED(GetProcessUser(&name, &domain, &sid));
  EXPECT_FALSE(name.empty());
  EXPECT_FALSE(domain.empty());
  EXPECT_FALSE(sid.empty());
}

// Tests that GetProcessUser succeeds when all output pointers are null.
TEST(UserInfoTest, GetProcessUserAllNullOutputs) {
  ASSERT_HRESULT_SUCCEEDED(GetProcessUser(nullptr, nullptr, nullptr));
}

// Tests that GetProcessUser returns a SID consistent with
// base::win::AccessToken.
TEST(UserInfoTest, GetProcessUserSidMatchesAccessToken) {
  std::wstring sid;
  ASSERT_HRESULT_SUCCEEDED(GetProcessUser(nullptr, nullptr, &sid));

  auto token = base::win::AccessToken::FromCurrentProcess();
  ASSERT_TRUE(token.has_value());
  auto expected_sddl = token->User().ToSddlString();
  ASSERT_TRUE(expected_sddl.has_value());

  EXPECT_EQ(sid, *expected_sddl);
}

// Tests that GetProcessUser returns consistent results across multiple calls.
TEST(UserInfoTest, GetProcessUserIsConsistent) {
  std::wstring name1, domain1, sid1;
  ASSERT_HRESULT_SUCCEEDED(GetProcessUser(&name1, &domain1, &sid1));

  std::wstring name2, domain2, sid2;
  ASSERT_HRESULT_SUCCEEDED(GetProcessUser(&name2, &domain2, &sid2));

  EXPECT_EQ(sid1, sid2);
  EXPECT_EQ(name1, name2);
  EXPECT_EQ(domain1, domain2);
}

// Tests that IsLocalSystemUser returns false for a normal user process.
// This test assumes it is not running as SYSTEM.
TEST(UserInfoTest, IsLocalSystemUserReturnsFalseForNormalUser) {
  EXPECT_FALSE(IsLocalSystemUser());
}

// Tests that GetThreadUserSid fails with an impersonation-related error
// when the thread is not impersonating.
TEST(UserInfoTest, GetThreadUserSidFailsWhenNotImpersonating) {
  std::wstring sid;
  // When the thread is not impersonating, there is no thread token, so
  // the call should fail.
  ASSERT_HRESULT_FAILED(GetThreadUserSid(&sid));
}

}  // namespace updater
