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

TEST_F(SyncUtilsTest, AreSigninAndSyncSetUpForSafeBrowsingTokenFetches) {
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env =
      std::make_unique<signin::IdentityTestEnvironment>();
  signin::IdentityManager* identity_manager =
      identity_test_env->identity_manager();
  syncer::TestSyncService sync_service;

  // Sync is disabled.
  sync_service.SetSignedOut();
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/true));
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/false));

  // Sync is enabled.
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "foo@gmail.com", signin::ConsentLevel::kSync);
  sync_service.SetSignedIn(signin::ConsentLevel::kSync, account_info);
  EXPECT_TRUE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/true));
  EXPECT_TRUE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/false));

  // History sync is disabled. Fetches are only allowed if the user enabled
  // enhanced protection explicitly.
  sync_service.GetUserSettings()->SetSelectedTypes(
      /* sync_everything */ false, {});
  EXPECT_TRUE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/true));
  EXPECT_FALSE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/false));

  // Custom passphrase is enabled. Fetches are only allowed if the user enabled
  // enhanced protection explicitly.
  sync_service.GetUserSettings()->SetSelectedTypes(
      false, {syncer::UserSelectableType::kHistory});
  sync_service.SetIsUsingExplicitPassphrase(true);
  EXPECT_TRUE(SyncUtils::AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      &sync_service, identity_manager,
      /* user_has_enabled_enhanced_protection=*/true));
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
      /*types=*/{syncer::UserSelectableType::kHistory});

  EXPECT_TRUE(SyncUtils::IsHistorySyncEnabled(&sync_service));

  // The method should return false if:

  // The |sync_service| is null.
  EXPECT_FALSE(SyncUtils::IsHistorySyncEnabled(nullptr));

  // History is not being synced.
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill});
  EXPECT_FALSE(SyncUtils::IsHistorySyncEnabled(&sync_service));

  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true,
      /*types=*/syncer::UserSelectableTypeSet::All());

  // Local sync is enabled.
  ASSERT_TRUE(SyncUtils::IsHistorySyncEnabled(&sync_service));
  sync_service.SetLocalSyncEnabled(true);
  EXPECT_FALSE(SyncUtils::IsHistorySyncEnabled(&sync_service));

  sync_service.SetLocalSyncEnabled(false);

  // The sync machinery is disabled for some reason (e.g. via enterprise
  // policy).
  ASSERT_TRUE(SyncUtils::IsHistorySyncEnabled(&sync_service));
  sync_service.SetAllowedByEnterprisePolicy(false);
  EXPECT_FALSE(SyncUtils::IsHistorySyncEnabled(&sync_service));
}

}  // namespace safe_browsing
