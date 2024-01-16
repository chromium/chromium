// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/os_gaia_user_manager.h"
#include "chrome/credential_provider/gaiacp/stdafx.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {
namespace testing {

class GcpOSGaiaUserManagerTest : public ::testing::Test {
 protected:
  FakeOSUserManager* fake_os_user_manager() { return &fake_os_user_manager_; }
  FakeScopedLsaPolicyFactory* fake_scoped_lsa_factory() {
    return &fake_scoped_lsa_factory_;
  }

 private:
  void SetUp() override;

  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_factory_;
};

void GcpOSGaiaUserManagerTest::SetUp() {
  FakesForTesting fakes;
  fakes.scoped_lsa_policy_creator =
      fake_scoped_lsa_factory()->GetCreatorCallback();
  fakes.os_user_manager_for_testing = fake_os_user_manager();

  OSGaiaUserManager::Get()->SetFakesForTesting(&fakes);
}

TEST_F(GcpOSGaiaUserManagerTest, CreateGaiaUserTest) {
  OSGaiaUserManager* manager = OSGaiaUserManager::Get();
  ASSERT_NE(manager, nullptr);

  auto policy = FakeScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  ASSERT_NE(policy, nullptr);

  PSID sid = nullptr;
  HRESULT hr = manager->CreateGaiaUser(&sid);
  EXPECT_EQ(S_OK, hr);
  EXPECT_FALSE(nullptr == sid);

  wchar_t gaia_username[kWindowsUsernameBufferLength] = {0};
  hr = policy->RetrievePrivateData(kLsaKeyGaiaUsername, gaia_username,
                                   std::size(gaia_username));
  EXPECT_EQ(S_OK, hr);

  wchar_t password[kWindowsPasswordBufferLength] = {0};
  hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, password,
                                   std::size(password));
  EXPECT_EQ(S_OK, hr);

  wchar_t stored_sid[kWindowsSidBufferLength] = {0};
  hr = policy->RetrievePrivateData(kLsaKeyGaiaSid, stored_sid,
                                   std::size(stored_sid));
  EXPECT_EQ(S_OK, hr);
}

TEST_F(GcpOSGaiaUserManagerTest, ChangeGaiaUserPasswordIfNeededNoStoredSid) {
  std::wstring password = L"asdfasdf";
  HRESULT hr;

  OSGaiaUserManager* manager = OSGaiaUserManager::Get();
  ASSERT_NE(manager, nullptr);

  auto policy = FakeScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  ASSERT_NE(policy, nullptr);

  hr = policy->StorePrivateData(kLsaKeyGaiaUsername, kDefaultGaiaAccountName);
  ASSERT_EQ(S_OK, hr);

  hr = policy->StorePrivateData(kLsaKeyGaiaPassword, password.c_str());
  ASSERT_EQ(S_OK, hr);

  CComBSTR local_sid;
  DWORD error;
  hr = fake_os_user_manager()->AddUser(kDefaultGaiaAccountName,
                                       password.c_str(), L"fullname",
                                       L"comment", true, &local_sid, &error);
  ASSERT_EQ(S_OK, hr);
  ASSERT_EQ(0u, error);

  hr = manager->ChangeGaiaUserPasswordIfNeeded();
  EXPECT_EQ(S_OK, hr);

  wchar_t new_password[kWindowsPasswordBufferLength];
  hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, new_password,
                                   std::size(new_password));

  ASSERT_EQ(S_OK, hr);

  EXPECT_STRNE(password.c_str(), new_password);

  wchar_t stored_gaia_sid[kWindowsSidBufferLength] = {0};
  hr = policy->RetrievePrivateData(kLsaKeyGaiaSid, stored_gaia_sid,
                                   std::size(stored_gaia_sid));
  ASSERT_EQ(S_OK, hr);

  std::wstring current_sid = OLE2CW(local_sid);
  EXPECT_STREQ(stored_gaia_sid, current_sid.c_str());
}

TEST_F(GcpOSGaiaUserManagerTest, ChangeGaiaUserPasswordIfNeededWithStoredSid) {
  OSGaiaUserManager* manager = OSGaiaUserManager::Get();
  ASSERT_NE(manager, nullptr);

  auto policy = FakeScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  ASSERT_NE(policy, nullptr);

  PSID sid = nullptr;
  HRESULT hr = manager->CreateGaiaUser(&sid);
  ASSERT_EQ(S_OK, hr);
  ASSERT_FALSE(nullptr == sid);

  wchar_t first_password[kWindowsPasswordBufferLength];
  hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, first_password,
                                   std::size(first_password));
  ASSERT_EQ(S_OK, hr);

  // Stored sid will be equal to current sid, so there will be no need to change
  // the password.
  hr = manager->ChangeGaiaUserPasswordIfNeeded();
  EXPECT_EQ(S_OK, hr);

  wchar_t second_password[kWindowsPasswordBufferLength];
  hr = policy->RetrievePrivateData(kLsaKeyGaiaPassword, second_password,
                                   std::size(second_password));
  ASSERT_EQ(S_OK, hr);

  // No password change.
  EXPECT_STREQ(first_password, second_password);
}

}  // namespace testing
}  // namespace credential_provider
