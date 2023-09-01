// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_sync_util.h"

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace password_manager {
namespace sync_util {

using PasswordSyncUtilTest = SyncUsernameTestBase;

PasswordForm SimpleGAIAChangePasswordForm() {
  PasswordForm form;
  form.url = GURL("https://myaccount.google.com/");
  form.signon_realm = "https://myaccount.google.com/";
  return form;
}

PasswordForm SimpleForm(const char* signon_realm, const char* username) {
  PasswordForm form;
  form.signon_realm = signon_realm;
  form.url = GURL(signon_realm);
  form.username_value = ASCIIToUTF16(username);
  return form;
}

TEST_F(PasswordSyncUtilTest, GetSyncUsernameIfSyncingPasswords) {
  const struct TestCase {
    enum { SYNCING_PASSWORDS, NOT_SYNCING_PASSWORDS } password_sync;
    std::string fake_sync_username;
    std::string expected_result;
    raw_ptr<const syncer::SyncService> sync_service;
    raw_ptr<const signin::IdentityManager> identity_manager;
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

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
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

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    SetSyncingPasswords(true);
    FakeSigninAs(kTestCases[i].fake_sync_username);
    EXPECT_EQ(kTestCases[i].expected_result,
              IsSyncAccountCredential(kTestCases[i].form.url,
                                      kTestCases[i].form.username_value,
                                      sync_service(), identity_manager()));
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

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    if (kTestCases[i].fake_sync_email.empty()) {
      EXPECT_EQ(kTestCases[i].expected_result,
                IsSyncAccountEmail(kTestCases[i].input_username, nullptr,
                                   signin::ConsentLevel::kSignin));
      continue;
    }
    FakeSigninAs(kTestCases[i].fake_sync_email);
    EXPECT_EQ(
        kTestCases[i].expected_result,
        IsSyncAccountEmail(kTestCases[i].input_username, identity_manager(),
                           signin::ConsentLevel::kSignin));
  }
}

TEST_F(PasswordSyncUtilTest, SyncDisabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetTransportState(syncer::SyncService::TransportState::DISABLED);
  sync_service.SetHasSyncConsent(false);
  EXPECT_FALSE(IsPasswordSyncEnabled(&sync_service));
  EXPECT_FALSE(IsPasswordSyncActive(&sync_service));
  EXPECT_EQ(absl::nullopt, GetSyncingAccount(&sync_service));
}

TEST_F(PasswordSyncUtilTest, SyncEnabledButNotForPasswords) {
  syncer::TestSyncService sync_service;
  sync_service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service.SetHasSyncConsent(true);
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kHistory});
  EXPECT_FALSE(IsPasswordSyncEnabled(&sync_service));
  EXPECT_FALSE(IsPasswordSyncActive(&sync_service));
  EXPECT_EQ(absl::nullopt, GetSyncingAccount(&sync_service));
}

TEST_F(PasswordSyncUtilTest, SyncEnabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service.SetHasSyncConsent(true);
  AccountInfo active_info;
  active_info.email = "test@email.com";
  sync_service.SetAccountInfo(active_info);
  EXPECT_TRUE(IsPasswordSyncEnabled(&sync_service));
  EXPECT_TRUE(IsPasswordSyncActive(&sync_service));
  EXPECT_TRUE(GetSyncingAccount(&sync_service).has_value());
  EXPECT_EQ(active_info.email, GetSyncingAccount(&sync_service).value());
}

TEST_F(PasswordSyncUtilTest, SyncPaused) {
  syncer::TestSyncService sync_service;
  sync_service.SetHasSyncConsent(true);
  sync_service.SetPersistentAuthError();
  ASSERT_EQ(sync_service.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  EXPECT_TRUE(IsPasswordSyncEnabled(&sync_service));
  EXPECT_FALSE(IsPasswordSyncActive(&sync_service));
  EXPECT_NE(absl::nullopt, GetSyncingAccount(&sync_service));
}

}  // namespace sync_util
}  // namespace password_manager
