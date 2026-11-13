// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/articles_list_visible_sync_service.h"

#include <memory>

#include "base/location.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

using FormFactor = ArticlesListVisibleSyncService::FormFactor;

syncer::SyncData CreateRemoteSyncData(const char* pref_name, bool visible) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* preference = specifics.mutable_preference();
  preference->set_name(pref_name);
  preference->set_value(visible ? "true" : "false");
  return syncer::SyncData::CreateRemoteData(
      specifics,
      syncer::ClientTagHash::FromUnhashed(syncer::PREFERENCES, pref_name));
}

// Uses a single-layer pref store, so the account storage specific behavior of
// not writing downloaded values back to the synced pref (which would copy them
// to the local layer) isn't observable here.
class ArticlesListVisibleSyncServiceTest : public testing::Test {
 public:
  ArticlesListVisibleSyncServiceTest() {
    prefs::RegisterFeedSharedProfilePrefs(prefs_.registry());
    ArticlesListVisibleSyncService::RegisterProfilePrefs(prefs_.registry());
  }

  ~ArticlesListVisibleSyncServiceTest() override {
    if (service_) {
      service_->Shutdown();
    }
  }

 protected:
  void CreateService(FormFactor form_factor) {
    service_ =
        std::make_unique<ArticlesListVisibleSyncService>(prefs_, form_factor);
  }

  bool IsArticlesListVisible() {
    return prefs_.GetBoolean(prefs::kArticlesListVisible);
  }

  void SetArticlesListVisible(bool visible) {
    prefs_.SetBoolean(prefs::kArticlesListVisible, visible);
  }

  syncer::SyncableService* GetSyncableService() {
    return prefs_.GetSyncableService(syncer::PREFERENCES);
  }

  void StartSyncing(const syncer::SyncDataList& initial_sync_data) {
    EXPECT_FALSE(GetSyncableService()
                     ->MergeDataAndStartSyncing(
                         syncer::PREFERENCES, initial_sync_data,
                         std::make_unique<syncer::FakeSyncChangeProcessor>())
                     .has_value());
  }

  void DownloadSyncedValue(const char* synced_pref_name, bool visible) {
    syncer::SyncChangeList changes = {
        syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                           CreateRemoteSyncData(synced_pref_name, visible))};
    EXPECT_FALSE(GetSyncableService()
                     ->ProcessSyncChanges(FROM_HERE, changes)
                     .has_value());
  }

  void TestLocalChangeIsSyncedImpl(FormFactor form_factor,
                                   const char* synced_pref_name,
                                   const char* other_synced_pref_name);
  void TestDownloadedValueIsAppliedImpl(FormFactor form_factor,
                                        const char* synced_pref_name,
                                        const char* other_synced_pref_name);

  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<ArticlesListVisibleSyncService> service_;
};

TEST_F(ArticlesListVisibleSyncServiceTest, GetFormFactor) {
  EXPECT_EQ(FormFactor::kPhone,
            ArticlesListVisibleSyncService::GetFormFactor(
                ui::DEVICE_FORM_FACTOR_PHONE, /*is_foldable=*/false));
  EXPECT_EQ(FormFactor::kTablet,
            ArticlesListVisibleSyncService::GetFormFactor(
                ui::DEVICE_FORM_FACTOR_TABLET, /*is_foldable=*/false));
  // Foldables sync with phones, whether they're folded or not.
  EXPECT_EQ(FormFactor::kPhone,
            ArticlesListVisibleSyncService::GetFormFactor(
                ui::DEVICE_FORM_FACTOR_PHONE, /*is_foldable=*/true));
  EXPECT_EQ(FormFactor::kPhone,
            ArticlesListVisibleSyncService::GetFormFactor(
                ui::DEVICE_FORM_FACTOR_TABLET, /*is_foldable=*/true));
  EXPECT_EQ(FormFactor::kPhone,
            ArticlesListVisibleSyncService::GetFormFactor(
                ui::DEVICE_FORM_FACTOR_FOLDABLE, /*is_foldable=*/false));

  for (ui::DeviceFormFactor device_form_factor :
       {ui::DEVICE_FORM_FACTOR_DESKTOP, ui::DEVICE_FORM_FACTOR_TV,
        ui::DEVICE_FORM_FACTOR_AUTOMOTIVE, ui::DEVICE_FORM_FACTOR_XR}) {
    EXPECT_EQ(std::nullopt, ArticlesListVisibleSyncService::GetFormFactor(
                                device_form_factor, /*is_foldable=*/false));
    EXPECT_EQ(std::nullopt, ArticlesListVisibleSyncService::GetFormFactor(
                                device_form_factor, /*is_foldable=*/true));
  }
}

TEST_F(ArticlesListVisibleSyncServiceTest, PhoneLocalChangeIsSynced) {
  TestLocalChangeIsSyncedImpl(FormFactor::kPhone,
                              prefs::kArticlesListVisibleOnPhone,
                              prefs::kArticlesListVisibleOnTablet);
}

TEST_F(ArticlesListVisibleSyncServiceTest, TabletLocalChangeIsSynced) {
  TestLocalChangeIsSyncedImpl(FormFactor::kTablet,
                              prefs::kArticlesListVisibleOnTablet,
                              prefs::kArticlesListVisibleOnPhone);
}

TEST_F(ArticlesListVisibleSyncServiceTest, PhoneDownloadedValueIsApplied) {
  TestDownloadedValueIsAppliedImpl(FormFactor::kPhone,
                                   prefs::kArticlesListVisibleOnPhone,
                                   prefs::kArticlesListVisibleOnTablet);
}

TEST_F(ArticlesListVisibleSyncServiceTest, TabletDownloadedValueIsApplied) {
  TestDownloadedValueIsAppliedImpl(FormFactor::kTablet,
                                   prefs::kArticlesListVisibleOnTablet,
                                   prefs::kArticlesListVisibleOnPhone);
}

TEST_F(ArticlesListVisibleSyncServiceTest, StartSyncingAppliesSyncedValue) {
  CreateService(FormFactor::kPhone);

  StartSyncing({CreateRemoteSyncData(prefs::kArticlesListVisibleOnPhone,
                                     /*visible=*/false)});

  EXPECT_FALSE(IsArticlesListVisible());
}

TEST_F(ArticlesListVisibleSyncServiceTest,
       StartSyncingAfterUploadAppliesMergedValue) {
  SetArticlesListVisible(/*visible=*/false);
  CreateService(FormFactor::kPhone);
  ASSERT_FALSE(prefs_.GetBoolean(prefs::kArticlesListVisibleOnPhone));

  StartSyncing({CreateRemoteSyncData(prefs::kArticlesListVisibleOnPhone,
                                     /*visible=*/true)});

  // Without account storage, the remote value wins the merge.
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kArticlesListVisibleOnPhone));
  EXPECT_TRUE(IsArticlesListVisible());
}

TEST_F(ArticlesListVisibleSyncServiceTest, StopSyncingKeepsLocalValue) {
  CreateService(FormFactor::kPhone);
  StartSyncing({});
  DownloadSyncedValue(prefs::kArticlesListVisibleOnPhone, /*visible=*/false);
  ASSERT_FALSE(IsArticlesListVisible());

  // Stopping sync, e.g. on sign-out, removes the downloaded value from the
  // account store, which changes the synced pref outside of sync.
  GetSyncableService()->StopSyncing(syncer::PREFERENCES);
  prefs_.ClearPref(prefs::kArticlesListVisibleOnPhone);

  EXPECT_FALSE(IsArticlesListVisible());
}

TEST_F(ArticlesListVisibleSyncServiceTest, CreationUploadsDifferentLocalValue) {
  SetArticlesListVisible(/*visible=*/false);

  CreateService(FormFactor::kPhone);

  EXPECT_FALSE(prefs_.GetBoolean(prefs::kArticlesListVisibleOnPhone));
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kArticlesListVisibleOnTablet));
}

TEST_F(ArticlesListVisibleSyncServiceTest, CreationDoesNotApplySyncedValue) {
  prefs_.SetBoolean(prefs::kArticlesListVisibleOnPhone, /*value=*/false);

  CreateService(FormFactor::kPhone);

  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kArticlesListVisible));
}

TEST_F(ArticlesListVisibleSyncServiceTest,
       CreationDoesNotUploadMatchingLocalValue) {
  SetArticlesListVisible(/*visible=*/true);

  CreateService(FormFactor::kPhone);

  // Uploading a value that matches the default would override other devices'
  // choices on sync merge.
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kArticlesListVisibleOnPhone));
}

TEST_F(ArticlesListVisibleSyncServiceTest,
       CreationWithoutUserValuesWritesNothing) {
  CreateService(FormFactor::kPhone);

  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kArticlesListVisible));
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kArticlesListVisibleOnPhone));
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kArticlesListVisibleOnTablet));
}

TEST_F(ArticlesListVisibleSyncServiceTest, ShutdownStopsSyncing) {
  CreateService(FormFactor::kPhone);
  StartSyncing({});
  service_->Shutdown();

  SetArticlesListVisible(/*visible=*/false);
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kArticlesListVisibleOnPhone));

  SetArticlesListVisible(/*visible=*/true);
  DownloadSyncedValue(prefs::kArticlesListVisibleOnPhone, /*visible=*/false);
  EXPECT_TRUE(IsArticlesListVisible());
}

void ArticlesListVisibleSyncServiceTest::TestLocalChangeIsSyncedImpl(
    FormFactor form_factor,
    const char* synced_pref_name,
    const char* other_synced_pref_name) {
  CreateService(form_factor);

  SetArticlesListVisible(/*visible=*/false);
  EXPECT_FALSE(prefs_.GetBoolean(synced_pref_name));
  EXPECT_FALSE(prefs_.HasPrefPath(other_synced_pref_name));

  SetArticlesListVisible(/*visible=*/true);
  EXPECT_TRUE(prefs_.GetBoolean(synced_pref_name));
  EXPECT_FALSE(prefs_.HasPrefPath(other_synced_pref_name));
}

void ArticlesListVisibleSyncServiceTest::TestDownloadedValueIsAppliedImpl(
    FormFactor form_factor,
    const char* synced_pref_name,
    const char* other_synced_pref_name) {
  CreateService(form_factor);
  StartSyncing({});

  // A value from devices of the other form factor is ignored.
  DownloadSyncedValue(other_synced_pref_name, /*visible=*/false);
  EXPECT_TRUE(IsArticlesListVisible());

  DownloadSyncedValue(synced_pref_name, /*visible=*/false);
  EXPECT_FALSE(IsArticlesListVisible());

  DownloadSyncedValue(synced_pref_name, /*visible=*/true);
  EXPECT_TRUE(IsArticlesListVisible());
}

}  // namespace
}  // namespace feed
