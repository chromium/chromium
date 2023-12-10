// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_sync_util.h"

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace sync_util {

using PasswordSyncUtilTest = SyncUsernameTestBase;

TEST_F(PasswordSyncUtilTest,
       GetAccountEmailIfSyncFeatureEnabledIncludingPasswords) {
  const struct TestCase {
    enum { SYNCING_PASSWORDS, NOT_SYNCING_PASSWORDS } password_sync;
    std::string fake_sync_username;
    std::string expected_result;
    raw_ptr<const syncer::SyncService> sync_service;
  } kTestCases[] = {
      {TestCase::NOT_SYNCING_PASSWORDS, "a@example.org", std::string(),
       sync_service()},

      {TestCase::SYNCING_PASSWORDS, "a@example.org", "a@example.org",
       sync_service()},

      {TestCase::NOT_SYNCING_PASSWORDS, "a@example.org", std::string(),
       nullptr},

      {TestCase::NOT_SYNCING_PASSWORDS, "a@example.org", std::string(),
       nullptr},
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
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
    FakeSigninAs(kTestCases[i].fake_sync_email, signin::ConsentLevel::kSync);
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
  EXPECT_FALSE(IsSyncFeatureEnabledIncludingPasswords(&sync_service));
  EXPECT_FALSE(IsSyncFeatureActiveIncludingPasswords(&sync_service));
  EXPECT_EQ(
      std::string(),
      GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(&sync_service));
}

TEST_F(PasswordSyncUtilTest, SyncEnabledButNotForPasswords) {
  syncer::TestSyncService sync_service;
  sync_service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service.SetHasSyncConsent(true);
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kHistory});
  EXPECT_FALSE(IsSyncFeatureEnabledIncludingPasswords(&sync_service));
  EXPECT_FALSE(IsSyncFeatureActiveIncludingPasswords(&sync_service));
  EXPECT_EQ(
      std::string(),
      GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(&sync_service));
}

TEST_F(PasswordSyncUtilTest, SyncEnabled) {
  syncer::TestSyncService sync_service;
  sync_service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service.SetHasSyncConsent(true);
  AccountInfo active_info;
  active_info.email = "test@email.com";
  sync_service.SetAccountInfo(active_info);
  EXPECT_TRUE(IsSyncFeatureEnabledIncludingPasswords(&sync_service));
  EXPECT_TRUE(IsSyncFeatureActiveIncludingPasswords(&sync_service));
  EXPECT_EQ(
      active_info.email,
      GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(&sync_service));
}

TEST_F(PasswordSyncUtilTest, SyncPaused) {
  syncer::TestSyncService sync_service;
  sync_service.SetHasSyncConsent(true);
  sync_service.SetPersistentAuthError();
  ASSERT_EQ(sync_service.GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  EXPECT_TRUE(IsSyncFeatureEnabledIncludingPasswords(&sync_service));
  EXPECT_FALSE(IsSyncFeatureActiveIncludingPasswords(&sync_service));
  EXPECT_NE(
      std::string(),
      GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(&sync_service));
}

}  // namespace sync_util
}  // namespace password_manager
