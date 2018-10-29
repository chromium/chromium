// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_sync_util.h"

#include <stddef.h>

#include "base/stl_util.h"
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

namespace password_manager {
namespace sync_util {

using PasswordSyncUtilTest = SyncUsernameTestBase;

PasswordForm SimpleGAIAChangePasswordForm() {
  PasswordForm form;
  form.signon_realm = "https://myaccount.google.com/";
  return form;
}

TEST_F(PasswordSyncUtilTest, GetSyncUsernameIfSyncingPasswords) {
  const struct TestCase {
    enum { SYNCING_PASSWORDS, NOT_SYNCING_PASSWORDS } password_sync;
    std::string fake_sync_username;
    std::string expected_result;
    const syncer::SyncService* sync_service;
    const SigninManagerBase* signin_manager;
  } kTestCases[] = {
      {TestCase::NOT_SYNCING_PASSWORDS, "a@example.org", std::string(),
       sync_service(), signin_manager()},

      {TestCase::SYNCING_PASSWORDS, "a@example.org", "a@example.org",
       sync_service(), signin_manager()},

      // If sync_service is not available, we assume passwords are synced, even
      // if they are not.
      {TestCase::NOT_SYNCING_PASSWORDS, "a@example.org", "a@example.org",
       nullptr, signin_manager()},

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
              GetSyncUsernameIfSyncingPasswords(kTestCases[i].sync_service,
                                                kTestCases[i].signin_manager));
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
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    SetSyncingPasswords(true);
    FakeSigninAs(kTestCases[i].fake_sync_username);
    EXPECT_EQ(kTestCases[i].expected_result,
              IsSyncAccountCredential(kTestCases[i].form, sync_service(),
                                      signin_manager()));
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
        IsSyncAccountEmail(kTestCases[i].input_username, signin_manager()));
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

TEST_F(PasswordSyncUtilEnterpriseTest, ShouldSavePasswordHash) {
  prefs_.SetString(prefs::kPasswordProtectionChangePasswordURL,
                   "https://pwchange.mydomain.com/");
  base::ListValue login_url;
  login_url.AppendString("https://login.mydomain.com/");
  prefs_.Set(prefs::kPasswordProtectionLoginURLs, login_url);
  const struct {
    PasswordForm form;
    std::string fake_sync_username;
    bool expected_result;
  } kTestCases[] = {
      {SimpleNonGaiaForm("sync_user@mydomain.com",
                         "https://pwchange.mydomain.com/"),
       "sync_user@mydomain.com", true},
      {SimpleNonGaiaForm("sync_user@mydomain.com",
                         "https://login.mydomain.com/"),
       "sync_user@mydomain.com", true},
      {SimpleNonGaiaForm("non_sync_user@mydomain.com",
                         "https://pwchange.mydomain.com/"),
       "sync_user@mydomain.com", false},
      {SimpleNonGaiaForm("non_sync_user@mydomain.com",
                         "https://login.mydomain.com/"),
       "sync_user@mydomain.com", false},
      {SimpleNonGaiaForm("sync_user", "https://pwchange.mydomain.com/"),
       "sync_user@mydomain.com", true},
      {SimpleNonGaiaForm("sync_user", "https://login.mydomain.com/"),
       "sync_user@mydomain.com", true},
      {SimpleNonGaiaForm("non_sync_user", "https://pwchange.mydomain.com/"),
       "sync_user@mydomain.com", false},
      {SimpleNonGaiaForm("non_sync_user", "https://login.mydomain.com/"),
       "sync_user@mydomain.com", false},
      {SimpleNonGaiaForm("", "https://pwchange.mydomain.com/"),
       "sync_user@mydomain.com", false},
      {SimpleNonGaiaForm("", "https://login.mydomain.com/"),
       "sync_user@mydomain.com", false},
      {SimpleNonGaiaForm("sync_user@mydomain.com", "https://otherdomain.com/"),
       "sync_user@mydomain.com", false},
      {SimpleNonGaiaForm("sync_user", "https://otherdomain.com/"),
       "sync_user@mydomain.com", false},
  };

  for (bool syncing_passwords : {false, true}) {
    for (size_t i = 0; i < base::size(kTestCases); ++i) {
      SCOPED_TRACE(testing::Message() << "i=" << i);
      SetSyncingPasswords(syncing_passwords);
      FakeSigninAs(kTestCases[i].fake_sync_username);
      EXPECT_EQ(kTestCases[i].expected_result,
                ShouldSavePasswordHash(kTestCases[i].form, signin_manager(),
                                       &prefs_));
    }
  }
}
#endif  // SYNC_PASSWORD_REUSE_DETECTION_ENABLED

}  // namespace sync_util
}  // namespace password_manager
