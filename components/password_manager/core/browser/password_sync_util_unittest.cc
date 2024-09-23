// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_sync_util.h"

#include <stddef.h>

#include <array>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager::sync_util {
namespace {

using PasswordSyncUtilTest = SyncUsernameTestBase;

TEST_F(PasswordSyncUtilTest,
       GetAccountEmailIfSyncFeatureEnabledIncludingPasswords) {
  struct TestCase {
    enum { SYNCING_PASSWORDS, NOT_SYNCING_PASSWORDS } password_sync;
    std::string fake_sync_username;
    std::string expected_result;
    raw_ptr<const syncer::SyncService> sync_service;
  };
  const auto kTestCases =
      std::to_array<TestCase>({{TestCase::NOT_SYNCING_PASSWORDS,
                                "a@example.org", std::string(), sync_service()},
                               {TestCase::SYNCING_PASSWORDS, "a@example.org",
                                "a@example.org", sync_service()},
                               {TestCase::NOT_SYNCING_PASSWORDS,
                                "a@example.org", std::string(), nullptr},
                               {TestCase::NOT_SYNCING_PASSWORDS,
                                "a@example.org", std::string(), nullptr}});

  for (size_t i = 0; i < kTestCases.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    SetSyncingPasswords(kTestCases[i].password_sync ==
                        TestCase::SYNCING_PASSWORDS);
    FakeSigninAs(kTestCases[i].fake_sync_username, signin::ConsentLevel::kSync);
    EXPECT_EQ(kTestCases[i].expected_result,
              GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
                  kTestCases[i].sync_service));
  }
}

TEST_F(PasswordSyncUtilTest, IsSyncAccountEmail) {
  struct TestCase {
    std::string fake_sync_email;
    std::string input_username;
    bool expected_result;
  };
  const auto kTestCases = std::to_array<TestCase>(
      {{"", "", false},
       {"", "user@example.org", false},
       {"sync_user@example.org", "", false},
       {"sync_user@example.org", "sync_user@example.org", true},
       {"sync_user@example.org", "sync_user", false},
       {"sync_user@example.org", "non_sync_user@example.org", false}});

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "i=" << i);
    if (kTestCases[i].fake_sync_email.empty()) {
      EXPECT_EQ(kTestCases[i].expected_result,
                IsSyncAccountEmail(kTestCases[i].input_username, nullptr,
                                   signin::ConsentLevel::kSignin));
      continue;
    }
    FakeSigninAs(kTestCases[i].fake_sync_email, signin::ConsentLevel::kSync);
    EXPECT_EQ(
        kTestCases[i].expected_result,
        IsSyncAccountEmail(kTestCases[i].input_username, identity_manager(),
                           signin::ConsentLevel::kSignin));
  }
}

TEST_F(PasswordSyncUtilTest, SignedOut) {
  test_sync_service()->SetSignedOut();
  EXPECT_FALSE(IsSyncFeatureEnabledIncludingPasswords(test_sync_service()));
  EXPECT_FALSE(HasChosenToSyncPasswords(test_sync_service()));
  EXPECT_FALSE(IsSyncFeatureActiveIncludingPasswords(test_sync_service()));
  EXPECT_EQ(std::string(),
            GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
                test_sync_service()));
  EXPECT_EQ(SyncState::kNotActive, GetPasswordSyncState(test_sync_service()));
}

TEST_F(PasswordSyncUtilTest, SyncEnabledButNotForPasswords) {
  test_sync_service()->SetSignedIn(signin::ConsentLevel::kSync);
  test_sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kHistory});
  EXPECT_FALSE(IsSyncFeatureEnabledIncludingPasswords(test_sync_service()));
  EXPECT_FALSE(HasChosenToSyncPasswords(test_sync_service()));
  EXPECT_FALSE(IsSyncFeatureActiveIncludingPasswords(test_sync_service()));
  EXPECT_EQ(std::string(),
            GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
                test_sync_service()));
  EXPECT_EQ(SyncState::kNotActive, GetPasswordSyncState(test_sync_service()));
}

TEST_F(PasswordSyncUtilTest, SyncEnabled) {
  AccountInfo active_info;
  active_info.email = "test@email.com";
  test_sync_service()->SetSignedIn(signin::ConsentLevel::kSync, active_info);
  EXPECT_TRUE(IsSyncFeatureEnabledIncludingPasswords(test_sync_service()));
  EXPECT_TRUE(HasChosenToSyncPasswords(test_sync_service()));
  EXPECT_TRUE(IsSyncFeatureActiveIncludingPasswords(test_sync_service()));
  EXPECT_EQ(active_info.email,
            GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
                test_sync_service()));
  EXPECT_EQ(SyncState::kActiveWithNormalEncryption,
            GetPasswordSyncState(test_sync_service()));
}

TEST_F(PasswordSyncUtilTest, SyncPaused) {
  test_sync_service()->SetSignedIn(signin::ConsentLevel::kSync);
  test_sync_service()->SetPersistentAuthError();
  ASSERT_EQ(test_sync_service()->GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  EXPECT_TRUE(IsSyncFeatureEnabledIncludingPasswords(test_sync_service()));
  EXPECT_TRUE(HasChosenToSyncPasswords(test_sync_service()));
  EXPECT_FALSE(IsSyncFeatureActiveIncludingPasswords(test_sync_service()));
  EXPECT_NE(std::string(),
            GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
                test_sync_service()));
  EXPECT_EQ(SyncState::kNotActive, GetPasswordSyncState(test_sync_service()));
}

TEST_F(PasswordSyncUtilTest, SyncEnabledWithCustomPassphrase) {
  AccountInfo active_info;
  active_info.email = "test@email.com";
  test_sync_service()->SetSignedIn(signin::ConsentLevel::kSync, active_info);
  test_sync_service()->SetIsUsingExplicitPassphrase(true);
  EXPECT_TRUE(IsSyncFeatureEnabledIncludingPasswords(test_sync_service()));
  EXPECT_TRUE(HasChosenToSyncPasswords(test_sync_service()));
  EXPECT_TRUE(IsSyncFeatureActiveIncludingPasswords(test_sync_service()));
  EXPECT_EQ(active_info.email,
            GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
                test_sync_service()));
  EXPECT_EQ(SyncState::kActiveWithCustomPassphrase,
            GetPasswordSyncState(test_sync_service()));
}

TEST_F(PasswordSyncUtilTest, SignedInWithPasswordsEnabled) {
  AccountInfo active_info;
  active_info.email = "test@email.com";
  test_sync_service()->SetSignedIn(signin::ConsentLevel::kSignin, active_info);
  EXPECT_FALSE(IsSyncFeatureEnabledIncludingPasswords(test_sync_service()));
  EXPECT_TRUE(HasChosenToSyncPasswords(test_sync_service()));
  EXPECT_FALSE(IsSyncFeatureActiveIncludingPasswords(test_sync_service()));
  EXPECT_EQ(std::string(),
            GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
                test_sync_service()));
  EXPECT_EQ(SyncState::kActiveWithNormalEncryption,
            GetPasswordSyncState(test_sync_service()));
}

TEST_F(PasswordSyncUtilTest, SignedInWithPasswordsDisabled) {
  test_sync_service()->SetSignedIn(signin::ConsentLevel::kSignin);
  test_sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);
  EXPECT_FALSE(IsSyncFeatureEnabledIncludingPasswords(test_sync_service()));
  EXPECT_FALSE(HasChosenToSyncPasswords(test_sync_service()));
  EXPECT_FALSE(IsSyncFeatureActiveIncludingPasswords(test_sync_service()));
  EXPECT_EQ(std::string(),
            GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
                test_sync_service()));
  EXPECT_EQ(SyncState::kNotActive, GetPasswordSyncState(test_sync_service()));
}

TEST_F(PasswordSyncUtilTest, SignedInWithCustomPassphrase) {
  AccountInfo active_info;
  active_info.email = "test@email.com";
  test_sync_service()->SetSignedIn(signin::ConsentLevel::kSignin, active_info);
  test_sync_service()->SetIsUsingExplicitPassphrase(true);
  EXPECT_FALSE(IsSyncFeatureEnabledIncludingPasswords(test_sync_service()));
  EXPECT_TRUE(HasChosenToSyncPasswords(test_sync_service()));
  EXPECT_FALSE(IsSyncFeatureActiveIncludingPasswords(test_sync_service()));
  EXPECT_EQ(std::string(),
            GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
                test_sync_service()));
  EXPECT_EQ(SyncState::kActiveWithCustomPassphrase,
            GetPasswordSyncState(test_sync_service()));
}

}  // namespace
}  // namespace password_manager::sync_util
