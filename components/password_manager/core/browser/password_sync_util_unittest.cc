// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_sync_util.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
#include "components/safe_browsing/common/safe_browsing_prefs.h"  // nogncheck
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED

using autofill::PasswordForm;
using base::ASCIIToUTF16;

namespace password_manager {
namespace sync_util {

using PasswordSyncUtilTest = SyncUsernameTestBase;

PasswordForm SimpleGAIAChangePasswordForm() {
  PasswordForm form;
  form.signon_realm = "https://myaccount.google.com/";
  return form;
}

PasswordForm SimpleForm(const char* signon_realm, const char* username) {
  PasswordForm form;
  form.signon_realm = signon_realm;
  form.username_value = ASCIIToUTF16(username);
  return form;
}

TEST_F(PasswordSyncUtilTest, GetSyncUsernameIfSyncingPasswords) {
  const struct TestCase {
    enum { SYNCING_PASSWORDS, NOT_SYNCING_PASSWORDS } password_sync;
    std::string fake_sync_username;
    std::string expected_result;
    const syncer::SyncService* sync_service;
    const signin::IdentityManager* identity_manager;
  } kTestCases[] = {
      {TestCase::NOT_SYNCING_PASSWORDS, "a@example.org", std::string(),
       sync_service(), identity_manager()},

      {TestCase::SYNCING_PASSWORDS, "a@example.org", "a@example.org",
       sync_service(), identity_manager()},

      // If sync_service is not available, we assume passwords are synced, even
      // if they are not.
      {TestCase::NOT_SYNCING_PASSWORDS, "a@example.org", "a@example.org",
       nullptr, identity_manager()},

      {TestCase::SYNCING_PASSWORDS, "a@example.org", std::string(),
       sync_service(), nullptr},

      {TestCase::SYNCING_PASSWORDS, "a@example.org", std::string(), nullptr,
       nullptr},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    SetSyncingPasswords(kTestCases[i].password_sync ==
                        TestCase::SYNCING_PASSWORDS);
    FakeSigninAs(kTestCases[i].fake_sync_username);
    EXPECT_EQ(kTestCases[i].expected_result,
              GetSyncUsernameIfSyncingPasswords(
                  kTestCases[i].sync_service, kTestCases[i].identity_manager));
  }
}

TEST_F(PasswordSyncUtilTest, IsSyncAccountCredential) {
  const struct {
    PasswordForm form;
    std::string fake_sync_username;
    bool expected_result;
  } kTestCases[] = {
      {SimpleGaiaForm("sync_user@example.org"), "sync_user@example.org", true},
      {SimpleGaiaForm("non_sync_user@example.org"), "sync_user@example.org",
       false},
      {SimpleNonGaiaForm("sync_user@example.org"), "sync_user@example.org",
       false},
      {SimpleGaiaForm(""), "sync_user@example.org", true},
      {SimpleNonGaiaForm(""), "sync_user@example.org", false},
      {SimpleGAIAChangePasswordForm(), "sync_user@example.org", true},
      {SimpleForm("https://subdomain.google.com/", "sync_user@example.org"),
       "sync_user@example.org", true},
      {SimpleForm("https://subdomain.google.com/", ""), "sync_user@example.org",
       true},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    SetSyncingPasswords(true);
    FakeSigninAs(kTestCases[i].fake_sync_username);
    EXPECT_EQ(kTestCases[i].expected_result,
              IsSyncAccountCredential(kTestCases[i].form, sync_service(),
                                      identity_manager()));
  }
}

TEST_F(PasswordSyncUtilTest, IsSyncAccountEmail) {
  const struct {
    std::string fake_sync_email;
    std::string input_username;
    bool expected_result;
  } kTestCases[] = {
      {"", "", false},
      {"", "user@example.org", false},
      {"sync_user@example.org", "", false},
      {"sync_user@example.org", "sync_user@example.org", true},
      {"sync_user@example.org", "sync_user", false},
      {"sync_user@example.org", "non_sync_user@example.org", false},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    if (kTestCases[i].fake_sync_email.empty()) {
      EXPECT_EQ(kTestCases[i].expected_result,
                IsSyncAccountEmail(kTestCases[i].input_username, nullptr));
      continue;
    }
    FakeSigninAs(kTestCases[i].fake_sync_email);
    EXPECT_EQ(
        kTestCases[i].expected_result,
        IsSyncAccountEmail(kTestCases[i].input_username, identity_manager()));
  }
}

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
class PasswordSyncUtilEnterpriseTest : public SyncUsernameTestBase {
 public:
  void SetUp() override {
    // prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_.registry()->RegisterListPref(prefs::kPasswordProtectionLoginURLs);
    prefs_.registry()->RegisterStringPref(
        prefs::kPasswordProtectionChangePasswordURL, "");
  }

 protected:
  TestingPrefServiceSimple prefs_;
};

#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED

}  // namespace sync_util
}  // namespace password_manager
