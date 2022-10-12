// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/sync/sync_utils.h"

#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class SyncUtilsTest : public PlatformTest {
 public:
  SyncUtilsTest() {}

  base::test::TaskEnvironment task_environment_;
};

TEST_F(SyncUtilsTest, AreSigninAndSyncSetUpForSafeBrowsingTokenFetches_Sync) {
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env =
      std::make_unique<signin::IdentityTestEnvironment>();
  signin::IdentityManager* identity_manager =
      identity_test_env->identity_manager();
  syncer::TestSyncService sync_service;

  // For the purposes of this test, IdentityManager has no primary account.

  // Sync is disabled.
  sync_service.SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_USER_CHOICE});
  sync_service.SetTransportState(syncer::SyncService::TransportState::DISABLED);
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/true));
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/false));

  // Sync is enabled.
  sync_service.SetDisableReasons({});
  sync_service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/true));
  EXPECT_TRUE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/false));

  // History sync is disabled.
  sync_service.GetUserSettings()->SetSelectedTypes(
      /* sync_everything */ false, {});
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/true));
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/false));

  // Custom passphrase is enabled.
  sync_service.GetUserSettings()->SetSelectedTypes(
      false, {syncer::UserSelectableType::kHistory});
  sync_service.SetIsUsingExplicitPassphrase(true);
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/true));
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/false));
}

TEST_F(SyncUtilsTest,
       AreSigninAndSyncSetUpForSafeBrowsingTokenFetches_IdentityManager) {
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env =
      std::make_unique<signin::IdentityTestEnvironment>();
  signin::IdentityManager* identity_manager =
      identity_test_env->identity_manager();
  syncer::TestSyncService sync_service;

  // For the purposes of this test, disable sync.
  sync_service.SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_USER_CHOICE});
  sync_service.SetTransportState(syncer::SyncService::TransportState::DISABLED);
  sync_service.GetUserSettings()->SetSelectedTypes(
      /* sync_everything */ false, {});

  // If the user is not signed in, it should not be
  // possible to perform URL lookups with tokens.
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/true));
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/false));

  // Enhanced protection is on and the user is signed in: it should be possible
  // to perform URL lookups with tokens (even though the
  // kRealTimeLookupEnabledWithToken feature and sync/history sync are
  // disabled).
  identity_test_env->MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);
  EXPECT_TRUE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/true));

  // Enhanced protection is *off* and the user is signed in: it should not be
  // possible to perform URL lookups with tokens without sync being enabled.
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/false));
}

TEST_F(SyncUtilsTest, IsHistorySyncEnabled) {
  syncer::TestSyncService sync_service;

  // By default |sync_service| syncs everything.
  EXPECT_TRUE(SyncUtils::IsHistorySyncEnabled(&sync_service));

  // Just history being synced should also be sufficient for the method to
  // return true.
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet(
          syncer::UserSelectableType::kHistory));

  EXPECT_TRUE(SyncUtils::IsHistorySyncEnabled(&sync_service));

  // The method should return false if:

  // The |sync_service| is null.
  EXPECT_FALSE(SyncUtils::IsHistorySyncEnabled(nullptr));

  // History is not being synced.
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet(
          syncer::UserSelectableType::kAutofill));
  EXPECT_FALSE(SyncUtils::IsHistorySyncEnabled(&sync_service));

  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true,
      /*types=*/syncer::UserSelectableTypeSet::All());

  // Local sync is enabled.
  sync_service.SetLocalSyncEnabled(true);
  EXPECT_FALSE(SyncUtils::IsHistorySyncEnabled(&sync_service));

  sync_service.SetLocalSyncEnabled(false);

  // The sync feature is disabled.
  sync_service.SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_USER_CHOICE});
  EXPECT_FALSE(SyncUtils::IsHistorySyncEnabled(&sync_service));

  sync_service.SetDisableReasons({});
}

}  // namespace safe_browsing
