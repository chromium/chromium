// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_service_utils.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledIfSyncNotAllowed) {
  TestSyncService service;

  // If sync is not allowed, uploading should never be enabled, even if all the
  // data types are enabled.
  service.SetAllowedByEnterprisePolicy(false);

  service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true,
      /*types=*/UserSelectableTypeSet::All());

  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Once sync gets allowed (e.g. policy is updated), uploading should not be
  // disabled anymore (though not necessarily active yet).
  service.SetAllowedByEnterprisePolicy(true);
  service.SetMaxTransportState(
      syncer::SyncService::TransportState::START_DEFERRED);

  EXPECT_NE(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest,
     UploadToGoogleInitializingUntilConfiguredAndActiveAndSyncCycleComplete) {
  TestSyncService service;
  service.SetMaxTransportState(
      syncer::SyncService::TransportState::START_DEFERRED);
  service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true,
      /*types=*/UserSelectableTypeSet::All());
  service.SetEmptyLastCycleSnapshot();

  // By default, if sync isn't disabled, we should be INITIALIZING.
  EXPECT_EQ(UploadState::INITIALIZING,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Finished configuration is not enough, still INITIALIZING.
  service.SetMaxTransportState(syncer::SyncService::TransportState::ACTIVE);
  EXPECT_EQ(UploadState::INITIALIZING,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Only after a sync cycle has been completed is upload actually ACTIVE.
  service.SetNonEmptyLastCycleSnapshot();
  EXPECT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledForDataType) {
  TestSyncService service;
  service.SetNonEmptyLastCycleSnapshot();

  // Sync is enabled only for a specific data type.
  service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kBookmarks});

  // Sanity check: Upload is ACTIVE for this data type.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // ...but not for other types.
  EXPECT_EQ(
      UploadState::NOT_ACTIVE,
      GetUploadToGoogleState(&service, syncer::HISTORY_DELETE_DIRECTIVES));
  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::PREFERENCES));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledForDataTypeThatFailedToStart) {
  TestSyncService service;
  service.SetNonEmptyLastCycleSnapshot();

  // Sync is enabled for some data types.
  service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kBookmarks,
                 syncer::UserSelectableType::kPreferences});

  // But one of them fails to actually start up!
  service.SetFailedDataTypes({syncer::PREFERENCES});

  // Sanity check: Upload is ACTIVE for the data type that did start up.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // ...but not for the type that failed.
  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::PREFERENCES));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledIfLocalSyncEnabled) {
  TestSyncService service;
  service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true,
      /*types=*/UserSelectableTypeSet::All());
  service.SetNonEmptyLastCycleSnapshot();

  // Sanity check: Upload is active now.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // If we're in "local sync" mode, uploading should never be enabled, even if
  // configuration is done and all the data types are enabled.
  service.SetLocalSyncEnabled(true);

  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledOnPersistentAuthError) {
  TestSyncService service;
  service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true,
      /*types=*/UserSelectableTypeSet::All());
  service.SetNonEmptyLastCycleSnapshot();

  // Sanity check: Upload is active now.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // On a persistent error, uploading is not considered active anymore (even
  // though Sync may still be considered active).
  service.SetPersistentAuthError();

  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Once the auth error is resolved (e.g. user re-authenticated), uploading is
  // active again.
  service.ClearAuthError();

  EXPECT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledIfCustomPassphraseInUse) {
  TestSyncService service;
  service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true,
      /*types=*/UserSelectableTypeSet::All());
  service.SetNonEmptyLastCycleSnapshot();

  // Sanity check: Upload is ACTIVE, even for data types that are always
  // encrypted implicitly (PASSWORDS).
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::PASSWORDS));
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::DEVICE_INFO));

  // Once a custom passphrase is in use, upload should be considered disabled:
  // Even if we're technically still uploading, Google can't inspect the data.
  service.SetIsUsingExplicitPassphrase(true);

  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::PASSWORDS));
  // But unencryptable types like DEVICE_INFO are still active.
  EXPECT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::DEVICE_INFO));
}

TEST(SyncServiceUtilsTest, UploadToGoogleEnabledInTransportMode) {
  TestSyncService service;
  service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true,
      /*types=*/UserSelectableTypeSet::All());
  service.SetNonEmptyLastCycleSnapshot();

  // Sanity check: Everything's looking good, so upload is considered active.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Sign in without kSync consent. With this, only Sync-the-transport (not
  // Sync-the-feature) can run.
  service.SetSignedIn(signin::ConsentLevel::kSignin);
  ASSERT_FALSE(service.CanSyncFeatureStart());

  // Regardless, if the datatype is active, it means uploading data is also
  // allowed.
  EXPECT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest, UploadToGoogleEnabledDespiteInitialSetupIncomplete) {
  TestSyncService service;
  service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true,
      /*types=*/UserSelectableTypeSet::All());
  service.SetNonEmptyLastCycleSnapshot();

  // Sanity check: Everything's looking good, so upload is considered active.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Clear the first-setup-complete bit.
  service.GetUserSettings()->ClearInitialSyncFeatureSetupComplete();

  // Regardless, if the datatype is active, it means uploading data is also
  // allowed.
  EXPECT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest, LocalSyncPreferredDataTypes) {
  TestSyncService service;
  service.SetSignedIn(signin::ConsentLevel::kSync);

  // By default local sync is disabled, so supported and non-supported types are
  // preferred.
  EXPECT_TRUE(service.GetPreferredDataTypes().Has(BOOKMARKS));
  EXPECT_TRUE(service.GetPreferredDataTypes().Has(HISTORY));

  // When local sync is enabled, only local sync supported types are preferred.
  service.SetLocalSyncEnabled(true);
  EXPECT_TRUE(service.GetPreferredDataTypes().Has(BOOKMARKS));
  EXPECT_FALSE(service.GetPreferredDataTypes().Has(HISTORY));
}

#if BUILDFLAG(IS_IOS)
TEST(SyncServiceUtilsTest, MaybeRecordIdentityErrorShown) {
  base::HistogramTester histogram_tester;
  MaybeRecordIdentityErrorShown(
      IdentityErrorDisplaySurface::kAccountMenu,
      SyncService::UserActionableError::kDeviceManagementError);
  histogram_tester.ExpectUniqueSample(
      "Signin.IdentityErrorDisplayed.DeviceManagementError",
      IdentityErrorDisplaySurface::kAccountMenu, 1);

  MaybeRecordIdentityErrorShown(
      IdentityErrorDisplaySurface::kSyncSettings,
      SyncService::UserActionableError::kDeviceManagementError);
  histogram_tester.ExpectBucketCount(
      "Signin.IdentityErrorDisplayed.DeviceManagementError",
      IdentityErrorDisplaySurface::kSyncSettings, 1);

  // Non-MDM errors should not be recorded by this histogram.
  MaybeRecordIdentityErrorShown(
      IdentityErrorDisplaySurface::kAccountMenu,
      SyncService::UserActionableError::kSignInNeedsUpdate);
  histogram_tester.ExpectTotalCount(
      "Signin.IdentityErrorDisplayed.DeviceManagementError", 2);
}
#endif  // BUILDFLAG(IS_IOS)

// Verifies that `IsSyncingUserSelectableTypesAllowedByPolicy()` fails closed
// when `SyncService` is null, even for an empty `UserSelectableTypeSet`.
TEST(SyncServiceUtilsTest,
     IsSyncingUserSelectableTypesAllowedByPolicyNullService) {
  EXPECT_FALSE(IsSyncingUserSelectableTypesAllowedByPolicy(
      /*sync_service=*/nullptr, {UserSelectableType::kTabs}));
  EXPECT_FALSE(IsSyncingUserSelectableTypesAllowedByPolicy(
      /*sync_service=*/nullptr, UserSelectableTypeSet()));
}

// Verifies that enterprise-level Sync disablement blocks policy eligibility
// while user-level deselection alone keeps types allowed by policy.
TEST(SyncServiceUtilsTest,
     IsSyncingUserSelectableTypesAllowedByPolicyEnterpriseDisabled) {
  TestSyncService service;
  const UserSelectableTypeSet kTypes = {UserSelectableType::kTabs,
                                        UserSelectableType::kHistory};

  // User-level deselection does not count as a policy restriction.
  service.GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                              UserSelectableTypeSet());
  EXPECT_TRUE(IsSyncingUserSelectableTypesAllowedByPolicy(&service, kTypes));

  // Enterprise-disabled Sync blocks both non-empty and empty type sets.
  service.SetAllowedByEnterprisePolicy(false);
  EXPECT_FALSE(IsSyncingUserSelectableTypesAllowedByPolicy(&service, kTypes));
  EXPECT_FALSE(IsSyncingUserSelectableTypesAllowedByPolicy(
      &service, UserSelectableTypeSet()));

  service.SetAllowedByEnterprisePolicy(true);
  EXPECT_TRUE(IsSyncingUserSelectableTypesAllowedByPolicy(&service, kTypes));
}

// Verifies per-type policy and custodian restrictions across single-type,
// multi-type, and empty `UserSelectableTypeSet` queries.
TEST(SyncServiceUtilsTest,
     IsSyncingUserSelectableTypesAllowedByPolicyPerTypePolicyAndCustodian) {
  TestSyncService service;
  const UserSelectableTypeSet kBothTypes = {UserSelectableType::kTabs,
                                            UserSelectableType::kHistory};

  ASSERT_TRUE(
      IsSyncingUserSelectableTypesAllowedByPolicy(&service, kBothTypes));

  // Policy restriction on `kHistory` blocks `{kTabs, kHistory}` and
  // `{kHistory}`, while `{kTabs}` and the empty set remain allowed.
  service.GetUserSettings()->SetTypeIsManagedByPolicy(
      UserSelectableType::kHistory, /*managed=*/true);
  EXPECT_FALSE(
      IsSyncingUserSelectableTypesAllowedByPolicy(&service, kBothTypes));
  EXPECT_FALSE(IsSyncingUserSelectableTypesAllowedByPolicy(
      &service, {UserSelectableType::kHistory}));
  EXPECT_TRUE(IsSyncingUserSelectableTypesAllowedByPolicy(
      &service, {UserSelectableType::kTabs}));
  EXPECT_TRUE(IsSyncingUserSelectableTypesAllowedByPolicy(
      &service, UserSelectableTypeSet()));

  // Custodian restriction on `kTabs` blocks `{kTabs, kHistory}` and `{kTabs}`,
  // while `{kHistory}` remains allowed once its policy restriction is cleared.
  service.GetUserSettings()->SetTypeIsManagedByPolicy(
      UserSelectableType::kHistory, /*managed=*/false);
  service.GetUserSettings()->SetTypeIsManagedByCustodian(
      UserSelectableType::kTabs, /*managed=*/true);
  EXPECT_FALSE(
      IsSyncingUserSelectableTypesAllowedByPolicy(&service, kBothTypes));
  EXPECT_FALSE(IsSyncingUserSelectableTypesAllowedByPolicy(
      &service, {UserSelectableType::kTabs}));
  EXPECT_TRUE(IsSyncingUserSelectableTypesAllowedByPolicy(
      &service, {UserSelectableType::kHistory}));
  EXPECT_TRUE(IsSyncingUserSelectableTypesAllowedByPolicy(
      &service, UserSelectableTypeSet()));
}

}  // namespace syncer
