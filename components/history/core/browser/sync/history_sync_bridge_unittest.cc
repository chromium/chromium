// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_sync_bridge.h"

#include <string>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "components/history/core/browser/sync/history_sync_metadata_database.h"
#include "components/history/core/browser/sync/test_history_backend_for_sync.h"
#include "components/sync/base/page_transition_conversion.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "components/sync/test/model/mock_model_type_change_processor.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

using testing::Return;

sync_pb::HistorySpecifics CreateSpecifics(
    base::Time visit_time,
    const std::string& originator_cache_guid,
    const GURL& url) {
  sync_pb::HistorySpecifics specifics;
  specifics.set_visit_time_windows_epoch_micros(
      visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_originator_cache_guid(originator_cache_guid);
  auto* url_entry = specifics.add_redirect_entries();
  url_entry->set_url(url.spec());
  return specifics;
}

syncer::EntityData SpecificsToEntityData(
    const sync_pb::HistorySpecifics& specifics) {
  syncer::EntityData data;
  *data.specifics.mutable_history() = specifics;
  return data;
}

class HistorySyncBridgeTest : public testing::Test {
 public:
  HistorySyncBridgeTest() : metadata_db_(&db_, &meta_table_) {}
  ~HistorySyncBridgeTest() override = default;

 protected:
  void SetUp() override {
    EXPECT_TRUE(db_.OpenInMemory());
    metadata_db_.Init();
    meta_table_.Init(&db_, /*version=*/1, /*compatible_version=*/1);

    // HistorySyncBridge never issues deletions (they're handled via
    // DeleteDirectives instead).
    EXPECT_CALL(*processor(), Delete).Times(0);

    // Creating the bridge triggers loading of the metadata, which is
    // synchronous.
    EXPECT_CALL(*processor(), ModelReadyToSync);
    bridge_ = std::make_unique<HistorySyncBridge>(
        &backend_, &metadata_db_, mock_processor_.CreateForwardingProcessor());
  }

  void TearDown() override {
    bridge_.reset();
    db_.Close();
  }

  TestHistoryBackendForSync* backend() { return &backend_; }
  syncer::MockModelTypeChangeProcessor* processor() { return &mock_processor_; }
  HistorySyncBridge* bridge() { return bridge_.get(); }

  syncer::EntityChangeList CreateAddEntityChangeList(
      const std::vector<sync_pb::HistorySpecifics>& specifics_vector) {
    syncer::EntityChangeList entity_change_list;
    for (const sync_pb::HistorySpecifics& specifics : specifics_vector) {
      syncer::EntityData data = SpecificsToEntityData(specifics);
      std::string storage_key = bridge_->GetStorageKey(data);
      entity_change_list.push_back(
          syncer::EntityChange::CreateAdd(storage_key, std::move(data)));
    }
    return entity_change_list;
  }

  void MergeSyncData(
      const std::vector<sync_pb::HistorySpecifics>& specifics_vector) {
    // Just before the merge, the processor starts tracking metadata.
    ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
    ON_CALL(*processor(), TrackedCacheGuid())
        .WillByDefault(Return("local_cache_guid"));

    absl::optional<syncer::ModelError> error =
        bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(),
                                CreateAddEntityChangeList(specifics_vector));
    if (error) {
      ADD_FAILURE() << "MergeSyncData failed: " << error->ToString();
    }
  }

  void ApplyStopSyncChanges() {
    syncer::MetadataBatch all_metadata;
    metadata_db_.GetAllSyncMetadata(&all_metadata);

    std::unique_ptr<syncer::MetadataChangeList> delete_all_metadata =
        bridge()->CreateMetadataChangeList();
    for (const auto& [storage_key, metadata] : all_metadata.GetAllMetadata()) {
      delete_all_metadata->ClearMetadata(storage_key);
    }
    delete_all_metadata->ClearModelTypeState();

    bridge()->ApplyStopSyncChanges(std::move(delete_all_metadata));

    // After stopping sync, metadata is not tracked anymore.
    ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(false));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  sql::Database db_;
  sql::MetaTable meta_table_;
  HistorySyncMetadataDatabase metadata_db_;

  TestHistoryBackendForSync backend_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;

  std::unique_ptr<HistorySyncBridge> bridge_;
};

TEST_F(HistorySyncBridgeTest, MergeDoesNotUploadData) {
  URLID url_id = backend()->AddURL(URLRow(GURL("https://www.url.com")));
  VisitRow visit;
  visit.url_id = url_id;
  visit.visit_time = base::Time::Now();
  backend()->AddVisit(visit);

  // The local data should *not* get uploaded to Sync.
  EXPECT_CALL(*processor(), Put).Times(0);

  MergeSyncData({});

  // The local data should still exist.
  EXPECT_EQ(backend()->GetURLs().size(), 1u);
  EXPECT_EQ(backend()->GetVisits().size(), 1u);
}

TEST_F(HistorySyncBridgeTest, MergeAppliesRemoteChanges) {
  const std::string remote_cache_guid("remote_cache_guid");
  const GURL local_url("https://local.com");
  const GURL remote_url("https://remote.com");

  URLID url_id = backend()->AddURL(URLRow(local_url));
  VisitRow visit;
  visit.url_id = url_id;
  visit.visit_time = base::Time::Now() - base::Minutes(5);
  backend()->AddVisit(visit);

  sync_pb::HistorySpecifics remote_entity = CreateSpecifics(
      base::Time::Now() - base::Minutes(1), remote_cache_guid, remote_url);

  MergeSyncData({remote_entity});

  // The local and remote data should both exist in the DB now.
  // Note: The ordering of the two entries in the backend doesn't really matter.
  ASSERT_EQ(backend()->GetURLs().size(), 2u);
  ASSERT_EQ(backend()->GetVisits().size(), 2u);
  EXPECT_EQ(backend()->GetURLs()[0].url(), local_url);
  EXPECT_EQ(backend()->GetURLs()[1].url(), remote_url);
  EXPECT_EQ(backend()->GetVisits()[0].url_id, backend()->GetURLs()[0].id());
  EXPECT_EQ(backend()->GetVisits()[1].url_id, backend()->GetURLs()[1].id());
  EXPECT_EQ(backend()->GetVisits()[1].originator_cache_guid, remote_cache_guid);
}

TEST_F(HistorySyncBridgeTest, MergeMergesRemoteChanges) {
  const GURL remote_url("https://remote.com");

  sync_pb::HistorySpecifics remote_entity = CreateSpecifics(
      base::Time::Now() - base::Minutes(1), "remote_cache_guid", remote_url);

  // Start Sync the first time, so the remote data gets written to the local DB.
  MergeSyncData({remote_entity});
  ASSERT_EQ(backend()->GetURLs().size(), 1u);
  ASSERT_EQ(backend()->GetVisits().size(), 1u);
  ASSERT_EQ(backend()->GetVisits()[0].visit_duration, base::TimeDelta());

  // Stop Sync, then start it again so the same data gets downloaded again.
  ApplyStopSyncChanges();
  // ...but the data has been updated in the meantime.
  remote_entity.set_visit_duration_micros(1000);
  MergeSyncData({remote_entity});

  // The entries in the local DB should have been updated (*not* duplicated).
  ASSERT_EQ(backend()->GetURLs().size(), 1u);
  ASSERT_EQ(backend()->GetVisits().size(), 1u);
  EXPECT_EQ(backend()->GetVisits()[0].visit_duration, base::Microseconds(1000));
}

TEST_F(HistorySyncBridgeTest, MergeIgnoresInvalidVisits) {
  const std::string remote_cache_guid("remote_cache_guid");
  const GURL remote_url("https://remote.com");

  // Create a bunch of remote entities that are invalid in various ways.
  sync_pb::HistorySpecifics missing_cache_guid =
      CreateSpecifics(base::Time::Now() - base::Minutes(1), "", remote_url);

  sync_pb::HistorySpecifics missing_visit_time =
      CreateSpecifics(base::Time(), remote_cache_guid, remote_url);
  missing_visit_time.clear_visit_time_windows_epoch_micros();

  sync_pb::HistorySpecifics no_redirects = CreateSpecifics(
      base::Time::Now() - base::Minutes(2), remote_cache_guid, remote_url);
  no_redirects.clear_redirect_entries();

  sync_pb::HistorySpecifics too_old = CreateSpecifics(
      base::Time::Now() - TestHistoryBackendForSync::kExpiryThreshold -
          base::Hours(1),
      remote_cache_guid, remote_url);

  sync_pb::HistorySpecifics too_new = CreateSpecifics(
      base::Time::Now() + base::Days(7), remote_cache_guid, remote_url);

  // ...and a single valid one.
  sync_pb::HistorySpecifics valid = CreateSpecifics(
      base::Time::Now() - base::Minutes(10), remote_cache_guid, remote_url);

  MergeSyncData({missing_cache_guid, missing_visit_time, no_redirects, too_old,
                 too_new, valid});

  // None of the invalid entities should've made it into the DB.
  ASSERT_EQ(backend()->GetURLs().size(), 1u);
  ASSERT_EQ(backend()->GetVisits().size(), 1u);
  EXPECT_EQ(backend()->GetURLs()[0].url(), remote_url);
  EXPECT_EQ(backend()->GetVisits()[0].originator_cache_guid, remote_cache_guid);
}

}  // namespace

}  // namespace history
