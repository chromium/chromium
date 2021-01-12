// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/sync/sync_utils.h"

#include "components/safe_browsing/core/common/test_task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class SyncUtilsTest : public PlatformTest {
 public:
  SyncUtilsTest() : task_environment_(CreateTestTaskEnvironment()) {}

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
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
  sync_service.SetIsUsingSecondaryPassphrase(true);
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
  identity_test_env->MakeUnconsentedPrimaryAccountAvailable("test@example.com");
  EXPECT_TRUE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/true));

  // Enhanced protection is *off* and the user is signed in: it should not be
  // possible to perform URL lookups with tokens without sync being enabled.
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/false));
}

}  // namespace safe_browsing
