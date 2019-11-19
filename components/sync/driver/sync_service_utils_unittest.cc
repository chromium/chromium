// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_utils.h"

#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledIfSyncNotAllowed) {
  TestSyncService service;

  // If sync is not allowed, uploading should never be enabled, even if all the
  // data types are enabled.
  service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  service.SetTransportState(syncer::SyncService::TransportState::DISABLED);

  service.SetPreferredDataTypes(ProtocolTypes());
  service.SetActiveDataTypes(ProtocolTypes());

  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Once sync gets allowed (e.g. policy is updated), uploading should not be
  // disabled anymore (though not necessarily active yet).
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetTransportState(
      syncer::SyncService::TransportState::START_DEFERRED);

  EXPECT_NE(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest,
     UploadToGoogleInitializingUntilConfiguredAndActiveAndSyncCycleComplete) {
  TestSyncService service;
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetTransportState(
      syncer::SyncService::TransportState::START_DEFERRED);
  service.SetPreferredDataTypes(ProtocolTypes());
  service.SetActiveDataTypes(ProtocolTypes());
  service.SetEmptyLastCycleSnapshot();

  // By default, if sync isn't disabled, we should be INITIALIZING.
  EXPECT_EQ(UploadState::INITIALIZING,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Finished configuration is not enough, still INITIALIZING.
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  EXPECT_EQ(UploadState::INITIALIZING,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Only after a sync cycle has been completed is upload actually ACTIVE.
  service.SetNonEmptyLastCycleSnapshot();
  EXPECT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledForModelType) {
  TestSyncService service;
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  service.SetNonEmptyLastCycleSnapshot();

  // Sync is enabled only for a specific model type.
  service.SetPreferredDataTypes(ModelTypeSet(syncer::BOOKMARKS));
  service.SetActiveDataTypes(ModelTypeSet(syncer::BOOKMARKS));

  // Sanity check: Upload is ACTIVE for this model type.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // ...but not for other types.
  EXPECT_EQ(
      UploadState::NOT_ACTIVE,
      GetUploadToGoogleState(&service, syncer::HISTORY_DELETE_DIRECTIVES));
  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::PREFERENCES));
}

TEST(SyncServiceUtilsTest,
     UploadToGoogleDisabledForModelTypeThatFailedToStart) {
  TestSyncService service;
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  service.SetNonEmptyLastCycleSnapshot();

  // Sync is enabled for some model types.
  service.SetPreferredDataTypes(
      ModelTypeSet(syncer::BOOKMARKS, syncer::PREFERENCES));
  // But one of them fails to actually start up!
  service.SetActiveDataTypes(ModelTypeSet(syncer::BOOKMARKS));

  // Sanity check: Upload is ACTIVE for the model type that did start up.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // ...but not for the type that failed.
  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::PREFERENCES));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledIfLocalSyncEnabled) {
  TestSyncService service;
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetPreferredDataTypes(ProtocolTypes());
  service.SetActiveDataTypes(ProtocolTypes());
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
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
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetPreferredDataTypes(ProtocolTypes());
  service.SetActiveDataTypes(ProtocolTypes());
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  service.SetNonEmptyLastCycleSnapshot();

  // Sanity check: Upload is active now.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // On a transient error, uploading goes back to INITIALIZING.
  GoogleServiceAuthError transient_error(
      GoogleServiceAuthError::CONNECTION_FAILED);
  ASSERT_TRUE(transient_error.IsTransientError());
  service.SetAuthError(transient_error);

  EXPECT_EQ(UploadState::INITIALIZING,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // On a persistent error, uploading is not considered active anymore (even
  // though Sync may still be considered active).
  GoogleServiceAuthError persistent_error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  ASSERT_TRUE(persistent_error.IsPersistentError());
  service.SetAuthError(persistent_error);

  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Once the auth error is resolved (e.g. user re-authenticated), uploading is
  // active again.
  service.SetAuthError(GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);

  EXPECT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledIfCustomPassphraseInUse) {
  TestSyncService service;
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetPreferredDataTypes(ProtocolTypes());
  service.SetActiveDataTypes(ProtocolTypes());
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
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
  service.SetIsUsingSecondaryPassphrase(true);

  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::PASSWORDS));
  // But unencryptable types like DEVICE_INFO are still active.
  EXPECT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::DEVICE_INFO));
}

TEST(SyncServiceUtilsTest, UploadToGoogleDisabledForSecondaryAccount) {
  TestSyncService service;
  service.SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
  service.SetPreferredDataTypes(ProtocolTypes());
  service.SetActiveDataTypes(ProtocolTypes());
  service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  service.SetNonEmptyLastCycleSnapshot();

  // Sanity check: Everything's looking good, so upload is considered active.
  ASSERT_EQ(UploadState::ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));

  // Mark the syncing account as non-primary. With this, only Sync-the-transport
  // (not Sync-the-feature) can run.
  service.SetIsAuthenticatedAccountPrimary(false);
  ASSERT_FALSE(service.CanSyncFeatureStart());

  // Upload should NOT be active now. Even though the data type is active, we're
  // running in standalone transport mode, so we don't have consent for
  // uploading.
  EXPECT_EQ(UploadState::NOT_ACTIVE,
            GetUploadToGoogleState(&service, syncer::BOOKMARKS));
}

}  // namespace syncer
