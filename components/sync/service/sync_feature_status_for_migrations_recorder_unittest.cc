// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_feature_status_for_migrations_recorder.h"

#include <memory>

#include "base/strings/strcat.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class SyncFeatureStatusForMigrationsRecorderTest : public testing::Test {
 public:
  SyncFeatureStatusForMigrationsRecorderTest() {
    SyncFeatureStatusForMigrationsRecorder::RegisterProfilePrefs(
        pref_service_.registry());
  }
  ~SyncFeatureStatusForMigrationsRecorderTest() override {
    if (recorder_) {
      recorder_->OnSyncShutdown(&sync_service_);
    }
  }

  void CreateRecorder() {
    if (recorder_) {
      recorder_->OnSyncShutdown(&sync_service_);
    }
    recorder_ = std::make_unique<SyncFeatureStatusForMigrationsRecorder>(
        &pref_service_, &sync_service_);
  }

  SyncFeatureStatusForSyncToSigninMigration GetSyncFeatureStatus() const {
    return SyncFeatureStatusForSyncToSigninMigrationFromInt(
        pref_service_.GetInteger(
            prefs::internal::kSyncFeatureStatusForSyncToSigninMigration));
  }

  bool GetDataTypeStatus(ModelType type) const {
    return pref_service_.GetBoolean(base::StrCat(
        {prefs::internal::kSyncDataTypeStatusForSyncToSigninMigrationPrefix,
         ".", GetModelTypeLowerCaseRootTag(type)}));
  }

  TestSyncService& sync_service() { return sync_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
  TestSyncService sync_service_;

  std::unique_ptr<SyncFeatureStatusForMigrationsRecorder> recorder_;
};

TEST_F(SyncFeatureStatusForMigrationsRecorderTest, AllEnabled) {
  // Sync-the-feature and all the data types are active.
  ASSERT_TRUE(sync_service().IsSyncFeatureActive());
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Has(syncer::BOOKMARKS));
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Has(syncer::READING_LIST));

  CreateRecorder();

  // The recorder should have marked everything as active.
  EXPECT_EQ(GetSyncFeatureStatus(),
            SyncFeatureStatusForSyncToSigninMigration::kActive);
  EXPECT_TRUE(GetDataTypeStatus(syncer::BOOKMARKS));
  EXPECT_TRUE(GetDataTypeStatus(syncer::PASSWORDS));
  EXPECT_TRUE(GetDataTypeStatus(syncer::READING_LIST));
}

TEST_F(SyncFeatureStatusForMigrationsRecorderTest, NoSyncConsent) {
  // Sync-the-feature is disabled, but all the data types are active (in
  // transport mode).
  sync_service().SetHasSyncConsent(false);
  ASSERT_FALSE(sync_service().IsSyncFeatureEnabled());
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Has(syncer::BOOKMARKS));
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Has(syncer::READING_LIST));

  CreateRecorder();

  // The recorder should have marked everything as disabled (even though the
  // data types were active!)
  EXPECT_EQ(GetSyncFeatureStatus(),
            SyncFeatureStatusForSyncToSigninMigration::kDisabledOrPaused);
  EXPECT_FALSE(GetDataTypeStatus(syncer::BOOKMARKS));
  EXPECT_FALSE(GetDataTypeStatus(syncer::PASSWORDS));
  EXPECT_FALSE(GetDataTypeStatus(syncer::READING_LIST));
}

TEST_F(SyncFeatureStatusForMigrationsRecorderTest, Initializing) {
  // Sync-the-feature is enabled, but the SyncService is still in the process
  // of initializing.
  sync_service().SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  ASSERT_TRUE(sync_service().IsSyncFeatureEnabled());
  ASSERT_FALSE(sync_service().IsSyncFeatureActive());
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Empty());

  CreateRecorder();

  // The recorder should have marked the overall status as initializing, and the
  // individual types still as off.
  EXPECT_EQ(GetSyncFeatureStatus(),
            SyncFeatureStatusForSyncToSigninMigration::kInitializing);
  EXPECT_FALSE(GetDataTypeStatus(syncer::BOOKMARKS));
  EXPECT_FALSE(GetDataTypeStatus(syncer::PASSWORDS));
  EXPECT_FALSE(GetDataTypeStatus(syncer::READING_LIST));
}

TEST_F(SyncFeatureStatusForMigrationsRecorderTest, TypesDisabled) {
  // Sync-the-feature is enabled, but BOOKMARKS is disabled by the user, and
  // PASSWORDS failed to start up.
  syncer::UserSelectableTypeSet selected_types =
      sync_service().GetUserSettings()->GetSelectedTypes();
  selected_types.Remove(syncer::UserSelectableType::kBookmarks);
  sync_service().GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                                     selected_types);
  sync_service().SetFailedDataTypes({syncer::PASSWORDS});

  ASSERT_TRUE(sync_service().IsSyncFeatureActive());
  ASSERT_FALSE(sync_service().GetActiveDataTypes().Has(syncer::BOOKMARKS));
  ASSERT_FALSE(sync_service().GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Has(syncer::READING_LIST));

  CreateRecorder();

  // The recorder should have marked the disabled/failed types (bookmarks and
  // passwords) as off, but e.g. reading list is still on.
  EXPECT_EQ(GetSyncFeatureStatus(),
            SyncFeatureStatusForSyncToSigninMigration::kActive);
  EXPECT_FALSE(GetDataTypeStatus(syncer::BOOKMARKS));
  EXPECT_FALSE(GetDataTypeStatus(syncer::PASSWORDS));
  EXPECT_TRUE(GetDataTypeStatus(syncer::READING_LIST));
}

TEST_F(SyncFeatureStatusForMigrationsRecorderTest, SyncPaused) {
  // Sync is paused due to an auth error.
  sync_service().SetPersistentAuthError();
  ASSERT_EQ(sync_service().GetTransportState(),
            syncer::SyncService::TransportState::PAUSED);
  ASSERT_TRUE(sync_service().IsSyncFeatureEnabled());
  ASSERT_FALSE(sync_service().IsSyncFeatureActive());
  ASSERT_FALSE(sync_service().GetActiveDataTypes().Has(syncer::BOOKMARKS));
  ASSERT_FALSE(sync_service().GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_FALSE(sync_service().GetActiveDataTypes().Has(syncer::READING_LIST));

  CreateRecorder();

  // The recorder should have marked everything as disabled.
  EXPECT_EQ(GetSyncFeatureStatus(),
            SyncFeatureStatusForSyncToSigninMigration::kDisabledOrPaused);
  EXPECT_FALSE(GetDataTypeStatus(syncer::BOOKMARKS));
  EXPECT_FALSE(GetDataTypeStatus(syncer::PASSWORDS));
  EXPECT_FALSE(GetDataTypeStatus(syncer::READING_LIST));
}

TEST_F(SyncFeatureStatusForMigrationsRecorderTest, StartupSequence) {
  // Initially, everything is enabled, but SyncService is still initializing.
  sync_service().SetTransportState(
      syncer::SyncService::TransportState::INITIALIZING);
  ASSERT_TRUE(sync_service().IsSyncFeatureEnabled());
  ASSERT_FALSE(sync_service().IsSyncFeatureActive());
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Empty());

  CreateRecorder();

  // The recorder should have marked the overall status as initializing, and the
  // individual types as off.
  EXPECT_EQ(GetSyncFeatureStatus(),
            SyncFeatureStatusForSyncToSigninMigration::kInitializing);
  EXPECT_FALSE(GetDataTypeStatus(syncer::BOOKMARKS));
  EXPECT_FALSE(GetDataTypeStatus(syncer::PASSWORDS));
  EXPECT_FALSE(GetDataTypeStatus(syncer::READING_LIST));

  // The SyncService moves on to "configuring". This shouldn't make a difference
  // to the recorder.
  sync_service().SetTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  sync_service().FireStateChanged();

  EXPECT_EQ(GetSyncFeatureStatus(),
            SyncFeatureStatusForSyncToSigninMigration::kInitializing);
  EXPECT_FALSE(GetDataTypeStatus(syncer::BOOKMARKS));
  EXPECT_FALSE(GetDataTypeStatus(syncer::PASSWORDS));
  EXPECT_FALSE(GetDataTypeStatus(syncer::READING_LIST));

  // Finally, the SyncService becomes active.
  sync_service().SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Has(syncer::BOOKMARKS));
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Has(syncer::PASSWORDS));
  ASSERT_TRUE(sync_service().GetActiveDataTypes().Has(syncer::READING_LIST));
  sync_service().FireStateChanged();

  EXPECT_EQ(GetSyncFeatureStatus(),
            SyncFeatureStatusForSyncToSigninMigration::kActive);
  EXPECT_TRUE(GetDataTypeStatus(syncer::BOOKMARKS));
  EXPECT_TRUE(GetDataTypeStatus(syncer::PASSWORDS));
  EXPECT_TRUE(GetDataTypeStatus(syncer::READING_LIST));
}

}  // namespace

}  // namespace syncer
