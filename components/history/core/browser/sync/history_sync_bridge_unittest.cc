// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_sync_bridge.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/sync/history_sync_metadata_database.h"
#include "components/history/core/browser/sync/test_history_backend_for_sync.h"
#include "components/history/core/browser/url_row.h"
#include "components/sync/base/page_transition_conversion.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/test/forwarding_data_type_local_change_processor.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

const std::string kTestAppId = "org.chromium.dino.stegosaurus";
const std::string kTestAppId2 = "org.chromium.dino.velociraptor";

namespace {

using testing::_;
using testing::Return;

GURL GetURL(int i) {
  return GURL(base::StringPrintf("https://url%i.com/", i));
}

sync_pb::HistorySpecifics CreateSpecifics(
    base::Time visit_time,
    const std::string& originator_cache_guid,
    const std::vector<GURL>& urls,
    const std::vector<VisitID>& originator_visit_ids = {},
    std::optional<std::string> app_id = std::nullopt,
    const bool has_url_keyed_image = false,
    const std::vector<VisitContentModelAnnotations::Category>& categories = {},
    const std::vector<std::string>& related_searches = {}) {
  DCHECK_EQ(originator_visit_ids.size(), urls.size());
  sync_pb::HistorySpecifics specifics;
  specifics.set_visit_time_windows_epoch_micros(
      visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_originator_cache_guid(originator_cache_guid);
  specifics.mutable_page_transition()->set_core_transition(
      sync_pb::SyncEnums_PageTransition_LINK);
  for (size_t i = 0; i < urls.size(); ++i) {
    auto* redirect_entry = specifics.add_redirect_entries();
    redirect_entry->set_originator_visit_id(originator_visit_ids[i]);
    redirect_entry->set_url(urls[i].spec());
    if (i > 0) {
      redirect_entry->set_redirect_type(
          sync_pb::SyncEnums_PageTransitionRedirectType_SERVER_REDIRECT);
    }
  }
  if (app_id) {
    specifics.set_app_id(*app_id);
  }
  specifics.set_has_url_keyed_image(has_url_keyed_image);
  for (const auto& category : categories) {
    auto* category_to_sync = specifics.add_categories();
    category_to_sync->set_id(category.id);
    category_to_sync->set_weight(category.weight);
  }
  specifics.mutable_related_searches()->Add(related_searches.begin(),
                                            related_searches.end());
  return specifics;
}

sync_pb::HistorySpecifics CreateSpecifics(
    base::Time visit_time,
    const std::string& originator_cache_guid,
    const GURL& url,
    VisitID originator_visit_id = 0,
    std::optional<std::string> app_id = std::nullopt,
    const bool has_url_keyed_image = false,
    const std::vector<VisitContentModelAnnotations::Category>& categories = {},
    const std::vector<std::string>& related_searches = {}) {
  return CreateSpecifics(visit_time, originator_cache_guid, std::vector{url},
                         std::vector{originator_visit_id}, app_id,
                         has_url_keyed_image, categories, related_searches);
}

syncer::EntityData SpecificsToEntityData(
    const sync_pb::HistorySpecifics& specifics) {
  syncer::EntityData data;
  *data.specifics.mutable_history() = specifics;
  return data;
}

class FakeDataTypeLocalChangeProcessor
    : public syncer::DataTypeLocalChangeProcessor {
 public:
  FakeDataTypeLocalChangeProcessor() = default;
  ~FakeDataTypeLocalChangeProcessor() override = default;

  void SetIsTrackingMetadata(bool is_tracking_metadata) {
    is_tracking_metadata_ = is_tracking_metadata;
  }

  void MarkEntitySynced(const std::string& storage_key) {
    DCHECK(unsynced_entities_.count(storage_key));
    unsynced_entities_.erase(storage_key);
  }

  void AddRemoteEntity(const std::string& storage_key,
                       syncer::EntityData entity_data) {
    // Remote entities are tracked, but *not* unsynced.
    tracked_entities_[storage_key] = entity_data.client_tag_hash;
    entities_[storage_key] = std::move(entity_data);
  }

  const std::map<std::string, syncer::EntityData>& GetEntities() const {
    return entities_;
  }

  void Put(const std::string& storage_key,
           std::unique_ptr<syncer::EntityData> entity_data,
           syncer::MetadataChangeList* metadata_change_list) override {
    // Update the persisted metadata.
    sync_pb::EntityMetadata metadata;
    metadata.set_sequence_number(1);
    metadata_change_list->UpdateMetadata(storage_key, metadata);
    // Store the entity, and mark it as tracked and unsynced.
    tracked_entities_[storage_key] = entity_data->client_tag_hash;
    unsynced_entities_.insert(storage_key);
    entities_[storage_key] = std::move(*entity_data);
  }

  void Delete(const std::string& storage_key,
              const syncer::DeletionOrigin& origin,
              syncer::MetadataChangeList* metadata_change_list) override {
    NOTREACHED_IN_MIGRATION();
  }

  void UpdateStorageKey(
      const syncer::EntityData& entity_data,
      const std::string& storage_key,
      syncer::MetadataChangeList* metadata_change_list) override {
    NOTREACHED_IN_MIGRATION();
  }

  void UntrackEntityForStorageKey(const std::string& storage_key) override {
    tracked_entities_.erase(storage_key);
    // If the entity was still unsynced, then this effectively deletes it (it
    // won't be committed), so also remove it from `entities_`.
    if (unsynced_entities_.count(storage_key)) {
      unsynced_entities_.erase(storage_key);
      entities_.erase(storage_key);
    }
  }

  void UntrackEntityForClientTagHash(
      const syncer::ClientTagHash& client_tag_hash) override {
    for (const auto& [storage_key, cth] : tracked_entities_) {
      if (cth == client_tag_hash) {
        UntrackEntityForStorageKey(storage_key);
        // Note: This modified `tracked_entities_`, so it's not safe to continue
        // the loop.
        break;
      }
    }
  }

  std::vector<std::string> GetAllTrackedStorageKeys() const override {
    std::vector<std::string> storage_keys;
    for (const auto& [storage_key, cth] : tracked_entities_) {
      storage_keys.push_back(storage_key);
    }
    return storage_keys;
  }

  bool IsEntityUnsynced(const std::string& storage_key) const override {
    return unsynced_entities_.count(storage_key) > 0;
  }

  base::Time GetEntityCreationTime(
      const std::string& storage_key) const override {
    NOTREACHED_IN_MIGRATION();
    return base::Time();
  }

  base::Time GetEntityModificationTime(
      const std::string& storage_key) const override {
    NOTREACHED_IN_MIGRATION();
    return base::Time();
  }

  void OnModelStarting(syncer::DataTypeSyncBridge* bridge) override {}

  void ModelReadyToSync(std::unique_ptr<syncer::MetadataBatch> batch) override {
  }

  bool IsTrackingMetadata() const override { return is_tracking_metadata_; }

  std::string TrackedAccountId() const override {
    if (!IsTrackingMetadata()) {
      return "";
    }
    return "account_id";
  }

  std::string TrackedCacheGuid() const override {
    if (!IsTrackingMetadata()) {
      return "";
    }
    return "local_cache_guid";
  }

  void ReportError(const syncer::ModelError& error) override {
    ADD_FAILURE() << "ReportError: " << error.ToString();
  }

  std::optional<syncer::ModelError> GetError() const override {
    return std::nullopt;
  }

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  const sync_pb::EntitySpecifics& GetPossiblyTrimmedRemoteSpecifics(
      const std::string& storage_key) const override {
    NOTREACHED_IN_MIGRATION();
    return sync_pb::EntitySpecifics::default_instance();
  }

  sync_pb::UniquePosition UniquePositionAfter(
      const std::string& storage_key_before,
      const syncer::ClientTagHash& target_client_tag_hash) const override {
    NOTREACHED();
  }
  sync_pb::UniquePosition UniquePositionBefore(
      const std::string& storage_key_after,
      const syncer::ClientTagHash& target_client_tag_hash) const override {
    NOTREACHED();
  }
  sync_pb::UniquePosition UniquePositionBetween(
      const std::string& storage_key_before,
      const std::string& storage_key_after,
      const syncer::ClientTagHash& target_client_tag_hash) const override {
    NOTREACHED();
  }
  sync_pb::UniquePosition UniquePositionForInitialEntity(
      const syncer::ClientTagHash& target_client_tag_hash) const override {
    NOTREACHED();
  }
  sync_pb::UniquePosition GetUniquePositionForStorageKey(
      const std::string& storage_key) const override {
    NOTREACHED();
  }

  base::WeakPtr<syncer::DataTypeLocalChangeProcessor> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  std::unique_ptr<DataTypeLocalChangeProcessor> CreateForwardingProcessor() {
    return base::WrapUnique<DataTypeLocalChangeProcessor>(
        new syncer::ForwardingDataTypeLocalChangeProcessor(this));
  }

 private:
  bool is_tracking_metadata_ = false;

  // Map from storage key to EntityData for all entities passed to Put().
  std::map<std::string, syncer::EntityData> entities_;

  // Map from storage key to ClientTagHash for all entities currently tracked by
  // the processor.
  std::map<std::string, syncer::ClientTagHash> tracked_entities_;

  // Set of storage keys of all unsynced entities (i.e. with local changes that
  // are pending commit).
  std::set<std::string> unsynced_entities_;

  base::WeakPtrFactory<FakeDataTypeLocalChangeProcessor> weak_ptr_factory_{
      this};
};

class HistorySyncBridgeTest : public testing::Test {
 public:
  HistorySyncBridgeTest() : metadata_db_(&db_, &meta_table_) {}
  ~HistorySyncBridgeTest() override = default;

 protected:
  void SetUp() override {
    EXPECT_TRUE(db_.OpenInMemory());
    metadata_db_.Init();
    ASSERT_TRUE(
        meta_table_.Init(&db_, /*version=*/1, /*compatible_version=*/1));

    // Creating the bridge triggers loading of the metadata, which is
    // synchronous.
    bridge_ = std::make_unique<HistorySyncBridge>(
        &backend_, &metadata_db_, fake_processor_.CreateForwardingProcessor());
  }

  void TearDown() override {
    bridge_.reset();
    db_.Close();
  }

  TestHistoryBackendForSync* backend() { return &backend_; }
  FakeDataTypeLocalChangeProcessor* processor() { return &fake_processor_; }
  HistorySyncBridge* bridge() { return bridge_.get(); }

  void AdvanceClock() { task_environment_.FastForwardBy(base::Seconds(1)); }

  std::pair<URLRow, VisitRow> AddVisitToBackendAndAdvanceClock(
      const GURL& url,
      ui::PageTransition transition,
      VisitID referring_visit = kInvalidVisitID,
      std::optional<std::string> app_id = std::nullopt) {
    // After grabbing the visit time, advance the mock time so that the next
    // visit will get a unique time.
    base::Time visit_time = base::Time::Now();
    AdvanceClock();

    URLRow url_row;
    const URLRow* existing_url_row = backend()->FindURLRow(url);
    if (existing_url_row) {
      url_row = *existing_url_row;
    } else {
      url_row.set_url(url);
      url_row.set_title(u"Title");
      url_row.set_id(backend()->AddURL(url_row));
    }

    VisitRow visit_row;
    visit_row.url_id = url_row.id();
    visit_row.visit_time = visit_time;
    visit_row.transition =
        ui::PageTransitionFromInt(transition | ui::PAGE_TRANSITION_CHAIN_START |
                                  ui::PAGE_TRANSITION_CHAIN_END);
    visit_row.referring_visit = referring_visit;
    visit_row.app_id = app_id;
    visit_row.visit_id = backend()->AddVisit(visit_row);

    return {url_row, visit_row};
  }

  syncer::EntityChangeList CreateAddEntityChangeList(
      const std::vector<sync_pb::HistorySpecifics>& specifics_vector) {
    syncer::EntityChangeList entity_change_list;
    for (const sync_pb::HistorySpecifics& specifics : specifics_vector) {
      syncer::EntityData data = SpecificsToEntityData(specifics);
      data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
          syncer::HISTORY, bridge_->GetClientTag(data));
      std::string storage_key = bridge_->GetStorageKey(data);
      entity_change_list.push_back(
          syncer::EntityChange::CreateAdd(storage_key, std::move(data)));
    }
    return entity_change_list;
  }

  void ApplyInitialSyncChanges(
      const std::vector<sync_pb::HistorySpecifics>& specifics_vector) {
    bridge()->SetSyncTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    bridge()->OnSyncStarting(syncer::DataTypeActivationRequest());

    // Just before passing on the initial updates, the processor starts tracking
    // metadata.
    processor()->SetIsTrackingMetadata(true);

    // Populate a MetadataChangeList with a DataTypeState, and an
    // EntityMetadata entry for each entity.
    std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
        bridge()->CreateMetadataChangeList();
    sync_pb::DataTypeState data_type_state;
    data_type_state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
    metadata_changes->UpdateDataTypeState(data_type_state);
    for (const sync_pb::HistorySpecifics& specifics : specifics_vector) {
      syncer::EntityData data = SpecificsToEntityData(specifics);
      data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
          syncer::HISTORY, bridge_->GetClientTag(data));
      std::string storage_key = bridge_->GetStorageKey(data);
      // Note: Don't bother actually populating the EntityMetadata - the bridge
      // doesn't inspect it anyway.
      metadata_changes->UpdateMetadata(storage_key, sync_pb::EntityMetadata());
      processor()->AddRemoteEntity(storage_key, std::move(data));
    }

    // Note that because HISTORY is in ApplyUpdatesImmediatelyTypes(), the
    // processor doesn't actually call MergeFullSyncData, but rather
    // ApplyIncrementalSyncChanges.
    std::optional<syncer::ModelError> error =
        bridge()->ApplyIncrementalSyncChanges(
            std::move(metadata_changes),
            CreateAddEntityChangeList(specifics_vector));
    if (error) {
      ADD_FAILURE() << "ApplyIncrementalSyncChanges failed: "
                    << error->ToString();
    }
  }

  void ApplyIncrementalSyncChanges(
      const std::vector<sync_pb::HistorySpecifics>& specifics_vector,
      const std::vector<std::string> extra_updated_metadata_storage_keys = {}) {
    // Populate a MetadataChangeList with the given updates/clears.
    std::unique_ptr<syncer::MetadataChangeList> metadata_changes =
        bridge()->CreateMetadataChangeList();
    // By default, add a metadata update for each new/changed entity.
    for (const sync_pb::HistorySpecifics& specifics : specifics_vector) {
      syncer::EntityData data = SpecificsToEntityData(specifics);
      data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
          syncer::HISTORY, bridge_->GetClientTag(data));
      std::string storage_key = bridge_->GetStorageKey(data);
      // Note: Don't bother actually populating the EntityMetadata - the bridge
      // doesn't inspect it anyway.
      metadata_changes->UpdateMetadata(storage_key, sync_pb::EntityMetadata());
      processor()->AddRemoteEntity(storage_key, std::move(data));
    }
    // Add additional metadata updates, if specified.
    for (const std::string& storage_key : extra_updated_metadata_storage_keys) {
      // Note: Don't bother actually populating the EntityMetadata - the bridge
      // doesn't inspect it anyway.
      metadata_changes->UpdateMetadata(storage_key, sync_pb::EntityMetadata());
    }

    std::optional<syncer::ModelError> error =
        bridge()->ApplyIncrementalSyncChanges(
            std::move(metadata_changes),
            CreateAddEntityChangeList(specifics_vector));
    if (error) {
      ADD_FAILURE() << "ApplyIncrementalSyncChanges failed: "
                    << error->ToString();
    }
  }

  void ApplyDisableSyncChanges() {
    syncer::MetadataBatch all_metadata;
    metadata_db_.GetAllSyncMetadata(&all_metadata);

    std::unique_ptr<syncer::MetadataChangeList> delete_all_metadata =
        bridge()->CreateMetadataChangeList();
    for (const auto& [storage_key, metadata] : all_metadata.GetAllMetadata()) {
      delete_all_metadata->ClearMetadata(storage_key);
    }
    delete_all_metadata->ClearDataTypeState();

    bridge()->ApplyDisableSyncChanges(std::move(delete_all_metadata));

    // After stopping sync, metadata is not tracked anymore.
    processor()->SetIsTrackingMetadata(false);
  }

  syncer::EntityMetadataMap GetPersistedEntityMetadata() {
    auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
    if (!metadata_db_.GetAllSyncMetadata(metadata_batch.get())) {
      ADD_FAILURE() << "Failed to read metadata from DB";
    }
    return metadata_batch->TakeAllMetadata();
  }

  sync_pb::DataTypeState GetPersistedDataTypeState() {
    auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
    if (!metadata_db_.GetAllSyncMetadata(metadata_batch.get())) {
      ADD_FAILURE() << "Failed to read metadata from DB";
    }
    return metadata_batch->GetDataTypeState();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  sql::Database db_;
  sql::MetaTable meta_table_;
  HistorySyncMetadataDatabase metadata_db_;

  TestHistoryBackendForSync backend_;

  FakeDataTypeLocalChangeProcessor fake_processor_;

  std::unique_ptr<HistorySyncBridge> bridge_;
};

TEST_F(HistorySyncBridgeTest, AppliesRemoteChanges) {
  const std::string remote_cache_guid("remote_cache_guid");
  const GURL local_url("https://local.com");
  const GURL remote_url("https://remote.com");
  const bool has_url_keyed_image(true);
  const std::string category_id_1 = "mid1";
  const int category_weight_1 = 1;
  const std::string category_id_2 = "mid2";
  const int category_weight_2 = 2;
  const std::vector<VisitContentModelAnnotations::Category> categories = {
      {category_id_1, category_weight_1}, {category_id_2, category_weight_2}};
  const std::string related_search_1 = "http://www.url2.com";
  const std::string related_search_2 = "http://www.url3.com";
  const std::vector<std::string> related_searches(
      {related_search_1, related_search_2});

  AddVisitToBackendAndAdvanceClock(local_url, ui::PAGE_TRANSITION_LINK,
                                   /*referring_visit=*/kInvalidVisitID,
                                   kTestAppId);

  sync_pb::HistorySpecifics remote_entity = CreateSpecifics(
      base::Time::Now() - base::Minutes(1), remote_cache_guid, remote_url, {},
      kTestAppId2, has_url_keyed_image, categories, related_searches);

  ApplyInitialSyncChanges({remote_entity});

  // The local and remote data should both exist in the DB now.
  // Note: The ordering of the two entries in the backend doesn't really matter.
  ASSERT_EQ(backend()->GetURLs().size(), 2u);
  ASSERT_EQ(backend()->GetVisits().size(), 2u);
  EXPECT_EQ(backend()->GetURLs()[0].url(), local_url);
  EXPECT_EQ(backend()->GetURLs()[1].url(), remote_url);
  EXPECT_EQ(backend()->GetVisits()[0].url_id, backend()->GetURLs()[0].id());
  EXPECT_FALSE(backend()->GetVisits()[0].is_known_to_sync);
  EXPECT_EQ(backend()->GetVisits()[1].url_id, backend()->GetURLs()[1].id());
  EXPECT_EQ(backend()->GetVisits()[1].originator_cache_guid, remote_cache_guid);
  EXPECT_TRUE(backend()->GetVisits()[1].is_known_to_sync);
  EXPECT_EQ(backend()->GetVisits()[0].app_id, kTestAppId);
  EXPECT_EQ(backend()->GetVisits()[1].app_id, kTestAppId2);

  // Check that the remote visit's annotation info got synced.
  // NOTE: Annotation info is present on the last remote visit.
  const std::vector<AnnotatedVisit> annotated_visits =
      backend()->ToAnnotatedVisitsFromRows(
          backend()->GetVisits(),
          /*compute_redirect_chain_start_properties=*/false);
  EXPECT_TRUE(annotated_visits[1].content_annotations.has_url_keyed_image);
  EXPECT_EQ(annotated_visits[1].content_annotations.related_searches.size(),
            2u);
  EXPECT_EQ(annotated_visits[1].content_annotations.related_searches[0],
            related_search_1);
  EXPECT_EQ(annotated_visits[1].content_annotations.related_searches[1],
            related_search_2);
  EXPECT_EQ(annotated_visits[1]
                .content_annotations.model_annotations.categories.size(),
            2u);
  EXPECT_EQ(annotated_visits[1]
                .content_annotations.model_annotations.categories[0]
                .id,
            category_id_1);
  EXPECT_EQ(annotated_visits[1]
                .content_annotations.model_annotations.categories[0]
                .weight,
            category_weight_1);
  EXPECT_EQ(annotated_visits[1]
                .content_annotations.model_annotations.categories[1]
                .id,
            category_id_2);
  EXPECT_EQ(annotated_visits[1]
                .content_annotations.model_annotations.categories[1]
                .weight,
            category_weight_2);
}

TEST_F(HistorySyncBridgeTest, MergesRemoteChanges) {
  const GURL remote_url("https://remote.com");

  sync_pb::HistorySpecifics remote_entity =
      CreateSpecifics(base::Time::Now() - base::Minutes(1), "remote_cache_guid",
                      remote_url, {}, kTestAppId);

  // Start Sync the first time, so the remote data gets written to the local DB.
  ApplyInitialSyncChanges({remote_entity});
  ASSERT_EQ(backend()->GetURLs().size(), 1u);
  ASSERT_EQ(backend()->GetVisits().size(), 1u);
  ASSERT_EQ(backend()->GetVisits()[0].visit_duration, base::TimeDelta());
  ASSERT_EQ(backend()->GetVisits()[0].app_id, kTestAppId);

  // Stop Sync, then start it again so the same data gets downloaded again.
  ApplyDisableSyncChanges();
  // ...but the data has been updated in the meantime.
  remote_entity.set_visit_duration_micros(1000);
  remote_entity.set_app_id(kTestAppId2);
  ApplyInitialSyncChanges({remote_entity});

  // The entries in the local DB should have been updated (*not* duplicated).
  ASSERT_EQ(backend()->GetURLs().size(), 1u);
  ASSERT_EQ(backend()->GetVisits().size(), 1u);
  EXPECT_EQ(backend()->GetVisits()[0].visit_duration, base::Microseconds(1000));
  ASSERT_EQ(backend()->GetVisits()[0].app_id, kTestAppId2);
}

TEST_F(HistorySyncBridgeTest, DoesNotApplyUnsyncableRemoteChanges) {
  // Add some "unsyncable" URLs on the server:
  // file:// URLs don't make sense to sync.
  sync_pb::HistorySpecifics remote_entity1 =
      CreateSpecifics(base::Time::Now() - base::Minutes(2), "remote_cache_guid",
                      GURL("file:///path/to/file"));
  // "data://" URLs can be arbitrarily large, and thus shouldn't be synced.
  sync_pb::HistorySpecifics remote_entity2 =
      CreateSpecifics(base::Time::Now() - base::Minutes(1), "remote_cache_guid",
                      GURL("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ=="));

  ApplyInitialSyncChanges({remote_entity1, remote_entity2});

  // Since all remote URLs were invalid, they should not have been added to the
  // backend.
  EXPECT_TRUE(backend()->GetURLs().empty());
}

TEST_F(HistorySyncBridgeTest, ClearsDataWhenSyncStopped) {
  const GURL local_url("https://local.com");
  const GURL remote_url("https://remote.com");

  sync_pb::HistorySpecifics remote_entity = CreateSpecifics(
      base::Time::Now() - base::Minutes(1), "remote_cache_guid", remote_url);

  // Start Sync, so the remote data gets written to the local DB.
  ApplyInitialSyncChanges({remote_entity});

  // Visit a URL and notify the bridge. This will become a pending commit, and
  // thus cause an EntityMetadata record to be persisted.
  auto [url_row, visit_row] =
      AddVisitToBackendAndAdvanceClock(local_url, ui::PAGE_TRANSITION_LINK);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row, visit_row);

  ASSERT_EQ(backend()->GetURLs().size(), 2u);
  ASSERT_EQ(backend()->GetVisits().size(), 2u);

  // Some Sync metadata should now exist (both a non-empty DataTypeState, and
  // an EntityMetadata record for the local visit).
  ASSERT_NE(GetPersistedDataTypeState().ByteSizeLong(), 0u);
  ASSERT_FALSE(GetPersistedEntityMetadata().empty());

  // Stop Sync.
  ApplyDisableSyncChanges();

  // Any Sync metadata should have been cleared.
  EXPECT_EQ(GetPersistedDataTypeState().ByteSizeLong(), 0u);
  EXPECT_TRUE(GetPersistedEntityMetadata().empty());

  // The local visit should still exist in the DB, but since Sync was stopped
  // permanently, the remote visit should've been cleared.
  ASSERT_EQ(backend()->GetVisits().size(), 1u);
}

TEST_F(HistorySyncBridgeTest, DeletesForeignVisitsWhenTypeStoppedPermanently) {
  sync_pb::HistorySpecifics remote_entity =
      CreateSpecifics(base::Time::Now() - base::Minutes(1), "remote_cache_guid",
                      GURL("https://remote.com"));

  // Start Sync, so the remote data gets written to the local DB.
  ApplyInitialSyncChanges({remote_entity});
  ASSERT_EQ(backend()->GetVisits().size(), 1u);

  // Stop the data type temporarily, i.e. without deleting metadata, and without
  // changing the transport state.
  bridge()->OnSyncPaused();  // No-op, but for the sake of a realistic sequence.

  // This should *not* have cleared foreign visits from the DB.
  EXPECT_EQ(backend()->delete_all_foreign_visits_call_count(), 0);

  // Resume syncing, then stop the data type permanently.
  bridge()->OnSyncStarting(syncer::DataTypeActivationRequest());
  ApplyDisableSyncChanges();

  // Now foreign visits should've been cleared.
  EXPECT_EQ(backend()->delete_all_foreign_visits_call_count(), 1);
}

TEST_F(HistorySyncBridgeTest, DeletesForeignVisitsWhenSyncStoppedPermanently) {
  sync_pb::HistorySpecifics remote_entity =
      CreateSpecifics(base::Time::Now() - base::Minutes(1), "remote_cache_guid",
                      GURL("https://remote.com"));

  // Start Sync, so the remote data gets written to the local DB.
  ApplyInitialSyncChanges({remote_entity});
  ASSERT_EQ(backend()->GetVisits().size(), 1u);

  // Enter the Sync-paused state.
  bridge()->SetSyncTransportState(syncer::SyncService::TransportState::PAUSED);
  bridge()->OnSyncPaused();  // No-op, but for the sake of a realistic sequence.

  // This should *not* have cleared foreign visits from the DB.
  EXPECT_EQ(backend()->delete_all_foreign_visits_call_count(), 0);

  // Resume syncing.
  bridge()->OnSyncStarting(syncer::DataTypeActivationRequest());
  bridge()->SetSyncTransportState(syncer::SyncService::TransportState::ACTIVE);

  // Stop Sync permanently.
  ApplyDisableSyncChanges();
  bridge()->SetSyncTransportState(
      syncer::SyncService::TransportState::DISABLED);

  // Now foreign visits should've been cleared.
  EXPECT_EQ(backend()->delete_all_foreign_visits_call_count(), 1);
}

TEST_F(HistorySyncBridgeTest, IgnoresInvalidVisits) {
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

  ApplyInitialSyncChanges({missing_cache_guid, missing_visit_time, no_redirects,
                           too_old, too_new, valid});

  // None of the invalid entities should've made it into the DB.
  ASSERT_EQ(backend()->GetURLs().size(), 1u);
  ASSERT_EQ(backend()->GetVisits().size(), 1u);
  EXPECT_EQ(backend()->GetURLs()[0].url(), remote_url);
  EXPECT_EQ(backend()->GetVisits()[0].originator_cache_guid, remote_cache_guid);
}

TEST_F(HistorySyncBridgeTest, UploadsNewLocalVisit) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Visit a URL.
  auto [url_row, visit_row] = AddVisitToBackendAndAdvanceClock(
      GURL("https://www.url.com"),
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));

  // Notify the bridge about the visit - it should be sent to the processor.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row, visit_row);

  const std::string storage_key =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(
          visit_row.visit_time);
  EXPECT_EQ(processor()->GetEntities().size(), 1u);
  ASSERT_EQ(processor()->GetEntities().count(storage_key), 1u);
  const syncer::EntityData& entity = processor()->GetEntities().at(storage_key);

  // Spot check some fields of the resulting entity.
  ASSERT_TRUE(entity.specifics.has_history());
  const sync_pb::HistorySpecifics& history = entity.specifics.history();
  EXPECT_EQ(base::Time::FromDeltaSinceWindowsEpoch(
                base::Microseconds(history.visit_time_windows_epoch_micros())),
            visit_row.visit_time);
  EXPECT_EQ(history.originator_cache_guid(), processor()->TrackedCacheGuid());
  ASSERT_EQ(history.redirect_entries_size(), 1);
  EXPECT_EQ(history.redirect_entries(0).url(), url_row.url());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      syncer::FromSyncPageTransition(
          history.page_transition().core_transition()),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(history.page_transition().forward_back());
  EXPECT_TRUE(history.page_transition().from_address_bar());

  // Re-fetch the visit from the backend and verify we've marked it as
  // `is_known_to_sync`.
  VisitRow visit_from_backend;
  ASSERT_TRUE(backend()->GetVisitByID(visit_row.visit_id, &visit_from_backend));
  EXPECT_TRUE(visit_from_backend.is_known_to_sync);
}

TEST_F(HistorySyncBridgeTest, DoesNotUploadPreexistingData) {
  auto [url_row, visit_row] = AddVisitToBackendAndAdvanceClock(
      GURL("https://www.url.com"), ui::PAGE_TRANSITION_LINK);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row, visit_row);

  ApplyInitialSyncChanges({});

  // The data should *not* have been uploaded to Sync.
  EXPECT_TRUE(processor()->GetEntities().empty());

  // The local data should still exist though.
  EXPECT_EQ(backend()->GetURLs().size(), 1u);
  EXPECT_EQ(backend()->GetVisits().size(), 1u);
}

TEST_F(HistorySyncBridgeTest, DoesNotUploadUnsyncableURLs) {
  ApplyInitialSyncChanges({});

  // file:// URLs don't make sense to sync.
  auto [url_row1, visit_row1] = AddVisitToBackendAndAdvanceClock(
      GURL("file:///path/to/file"), ui::PAGE_TRANSITION_TYPED);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row1, visit_row1);

  // "data://" URLs can be arbitrarily large, and thus shouldn't be synced.
  auto [url_row2, visit_row2] = AddVisitToBackendAndAdvanceClock(
      GURL("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ=="),
      ui::PAGE_TRANSITION_TYPED);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row2, visit_row2);

  // Note: There are several other types of URLs that shouldn't be synced, but
  // which are already filtered out by the history system before ever reaching
  // the bridge, such as javascript://, about://, chrome:// etc - see
  // CanAddURLToHistory().

  // The data should *not* have been uploaded to Sync.
  EXPECT_TRUE(processor()->GetEntities().empty());

  // Re-fetch these visits from the backend and verify we've NOT marked them as
  // `is_known_to_sync`.
  VisitRow visit_from_backend_1;
  ASSERT_TRUE(
      backend()->GetVisitByID(visit_row1.visit_id, &visit_from_backend_1));
  EXPECT_FALSE(visit_from_backend_1.is_known_to_sync);
  VisitRow visit_from_backend_2;
  ASSERT_TRUE(
      backend()->GetVisitByID(visit_row2.visit_id, &visit_from_backend_2));
  EXPECT_FALSE(visit_from_backend_2.is_known_to_sync);
}

TEST_F(HistorySyncBridgeTest, DoesNotUploadWhileSyncIsPaused) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Visit a URL and notify the bridge.
  auto [url_row1, visit_row1] = AddVisitToBackendAndAdvanceClock(
      GURL("https://www.url1.com"), ui::PAGE_TRANSITION_LINK);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row1, visit_row1);

  // Make sure it made it to the processor.
  const std::string storage_key1 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(
          visit_row1.visit_time);
  EXPECT_EQ(processor()->GetEntities().size(), 1u);
  EXPECT_EQ(processor()->GetEntities().count(storage_key1), 1u);

  // Stop Sync temporarily - this happens e.g. in the "Sync paused" case, i.e.
  // when the user signs out from the web.
  bridge()->OnSyncPaused();  // No-op, but for the sake of a realistic sequence.
  bridge()->SetSyncTransportState(syncer::SyncService::TransportState::PAUSED);
  // Note that IsTrackingMetadata() remains true - Sync is still enabled in
  // principle, just temporarily stopped.
  ASSERT_TRUE(processor()->IsTrackingMetadata());

  // Visit a URL while Sync is paused.
  auto [url_row2, visit_row2] = AddVisitToBackendAndAdvanceClock(
      GURL("https://www.url2.com"), ui::PAGE_TRANSITION_LINK);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row2, visit_row2);

  // Make sure this one did *not* make it to the processor.
  const std::string storage_key2 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(
          visit_row2.visit_time);
  EXPECT_EQ(processor()->GetEntities().size(), 1u);
  EXPECT_EQ(processor()->GetEntities().count(storage_key2), 0u);

  // Un-pause Sync.
  bridge()->SetSyncTransportState(syncer::SyncService::TransportState::ACTIVE);
  bridge()->OnSyncStarting(syncer::DataTypeActivationRequest());

  // Visit yet another URL.
  auto [url_row3, visit_row3] = AddVisitToBackendAndAdvanceClock(
      GURL("https://www.url3.com"), ui::PAGE_TRANSITION_LINK);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row3, visit_row3);

  // This one should've made it to the processor again.
  const std::string storage_key3 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(
          visit_row3.visit_time);
  EXPECT_EQ(processor()->GetEntities().size(), 2u);
  EXPECT_EQ(processor()->GetEntities().count(storage_key3), 1u);
}

TEST_F(HistorySyncBridgeTest, DoesNotUploadIfSyncIsPausedAtStartup) {
  // Sync is enabled (IsTrackingMetadata() is true), but paused.
  processor()->SetIsTrackingMetadata(true);
  bridge()->SetSyncTransportState(syncer::SyncService::TransportState::PAUSED);

  // Visit a URL and notify the bridge.
  auto [url_row, visit_row] = AddVisitToBackendAndAdvanceClock(
      GURL("https://www.url.com"), ui::PAGE_TRANSITION_LINK);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row, visit_row);

  // This should *not* have been sent to the processor.
  EXPECT_TRUE(processor()->GetEntities().empty());

  // Eventually Sync starts up, but this doesn't change anything.
  bridge()->SetSyncTransportState(syncer::SyncService::TransportState::ACTIVE);
  bridge()->OnSyncStarting(syncer::DataTypeActivationRequest());
  EXPECT_TRUE(processor()->GetEntities().empty());
}

TEST_F(HistorySyncBridgeTest, UploadsChangeFromBeforeSyncWasStarted) {
  // Sync is enabled (IsTrackingMetadata() is true), but hasn't started yet
  // (OnSyncStarting() hasn't been called).
  processor()->SetIsTrackingMetadata(true);
  bridge()->SetSyncTransportState(syncer::SyncService::TransportState::ACTIVE);

  // Visit a URL and notify the bridge.
  auto [url_row, visit_row] = AddVisitToBackendAndAdvanceClock(
      GURL("https://www.url.com"), ui::PAGE_TRANSITION_LINK);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row, visit_row);

  // Even though Sync hasn't started yet (and the bridge doesn't know whether
  // it's paused), this should have been sent to the processor.
  const std::string storage_key =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(
          visit_row.visit_time);
  EXPECT_EQ(processor()->GetEntities().size(), 1u);
  EXPECT_EQ(processor()->GetEntities().count(storage_key), 1u);

  // Now Sync starts up.
  bridge()->OnSyncStarting(syncer::DataTypeActivationRequest());

  // The entity should still be there (and the processor would commit it soon).
  EXPECT_EQ(processor()->GetEntities().size(), 1u);
}

TEST_F(HistorySyncBridgeTest, UploadsReferrerURL) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Visit a first visit (which will become the referrer).
  auto [url_row1, visit_row1] = AddVisitToBackendAndAdvanceClock(
      GURL("https://www.referrer.com"),
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row1, visit_row1);

  // Visit a second visit, which has the first one as its referrer.
  auto [url_row2, visit_row2] = AddVisitToBackendAndAdvanceClock(
      GURL("https://www.url.com"),
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK),
      /*referring_visit=*/visit_row1.visit_id);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row2, visit_row2);

  // Check that the second entity has the first one as its referrer, with both
  // visit ID and the actual URL.
  ASSERT_EQ(processor()->GetEntities().size(), 2u);
  const std::string storage_key2 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(
          visit_row2.visit_time);
  ASSERT_EQ(processor()->GetEntities().count(storage_key2), 1u);
  const syncer::EntityData& entity2 =
      processor()->GetEntities().at(storage_key2);
  ASSERT_TRUE(entity2.specifics.has_history());
  const sync_pb::HistorySpecifics& history2 = entity2.specifics.history();
  EXPECT_EQ(history2.originator_referring_visit_id(), visit_row1.visit_id);
  EXPECT_NE(history2.originator_cluster_id(), 0);
  EXPECT_EQ(history2.referrer_url(), url_row1.url());
}

TEST_F(HistorySyncBridgeTest, UploadsUpdatedLocalVisit) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Visit a URL.
  auto [url_row, visit_row] = AddVisitToBackendAndAdvanceClock(
      GURL("https://www.url.com"), ui::PAGE_TRANSITION_TYPED);

  // Notify the bridge about the visit - it should be sent to the processor.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row, visit_row);

  const std::string storage_key =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(
          visit_row.visit_time);
  EXPECT_EQ(processor()->GetEntities().size(), 1u);
  EXPECT_EQ(processor()->GetEntities().count(storage_key), 1u);

  // Update the visit by adding a duration.
  const base::TimeDelta visit_duration = base::Seconds(10);
  visit_row.visit_duration = visit_duration;
  ASSERT_TRUE(backend()->UpdateVisit(visit_row));
  bridge()->OnVisitUpdated(visit_row, VisitUpdateReason::kUpdateVisitDuration);

  // The updated data should have been sent to the processor.
  EXPECT_EQ(processor()->GetEntities().size(), 1u);
  ASSERT_EQ(processor()->GetEntities().count(storage_key), 1u);
  const syncer::EntityData& entity = processor()->GetEntities().at(storage_key);
  EXPECT_EQ(
      base::Microseconds(entity.specifics.history().visit_duration_micros()),
      visit_duration);
}

TEST_F(HistorySyncBridgeTest, IgnoresUninterestingVisitUpdate) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Visit a URL and notify the bridge.
  auto [url_row, visit_row] = AddVisitToBackendAndAdvanceClock(
      GURL("https://www.url.com"), ui::PAGE_TRANSITION_TYPED);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row, visit_row);

  const std::string storage_key =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(
          visit_row.visit_time);

  // The visit should've been sent to the processor. Mark it as "synced",
  // simulating that it was sent to the server.
  ASSERT_TRUE(processor()->IsEntityUnsynced(storage_key));
  processor()->MarkEntitySynced(storage_key);
  ASSERT_FALSE(processor()->IsEntityUnsynced(storage_key));

  // Notify the bridge about an uninteresting visit update (uninteresting since
  // none of the on-close context annotation fields are synced).
  bridge()->OnVisitUpdated(visit_row,
                           VisitUpdateReason::kSetOnCloseContextAnnotations);

  // This should *not* have been sent to the processor, so the entity should not
  // be unsynced now.
  EXPECT_FALSE(processor()->IsEntityUnsynced(storage_key));

  // Sanity check: Some other visit update *should* be sent to the processor.
  bridge()->OnVisitUpdated(visit_row,
                           VisitUpdateReason::kAddContextAnnotations);
  EXPECT_TRUE(processor()->IsEntityUnsynced(storage_key));
}

TEST_F(HistorySyncBridgeTest, DoesNotUploadUpdatedForeignVisit) {
  sync_pb::HistorySpecifics remote_entity =
      CreateSpecifics(base::Time::Now() - base::Minutes(1), "remote_cache_guid",
                      GURL("https://remote.com"));

  // Start Sync, so the remote data gets written to the local DB.
  ApplyInitialSyncChanges({remote_entity});
  ASSERT_EQ(backend()->GetVisits().size(), 1u);

  VisitRow visit_row = backend()->GetVisits()[0];
  const std::string storage_key =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(
          visit_row.visit_time);

  // The visit is known in the processor (representing the server state).
  ASSERT_EQ(processor()->GetEntities().size(), 1u);
  ASSERT_EQ(processor()->GetEntities().count(storage_key), 1u);
  ASSERT_FALSE(processor()->IsEntityUnsynced(storage_key));
  ASSERT_EQ(processor()
                ->GetEntities()
                .at(storage_key)
                .specifics.history()
                .visit_duration_micros(),
            0);

  // Update the foreign visit locally. Generally, foreign visits shouldn't get
  // updated on this device, but some other code interacting with the history DB
  // might do it (probably mistakenly).
  visit_row.visit_duration = base::Seconds(10);
  ASSERT_TRUE(backend()->UpdateVisit(visit_row));
  bridge()->OnVisitUpdated(visit_row, VisitUpdateReason::kUpdateVisitDuration);

  // The updated visit should *not* have been sent to the processor - the entity
  // in the processor should *not* be unsynced, and its visit duration should
  // still be 0.
  ASSERT_EQ(processor()->GetEntities().size(), 1u);
  ASSERT_EQ(processor()->GetEntities().count(storage_key), 1u);
  EXPECT_FALSE(processor()->IsEntityUnsynced(storage_key));
  EXPECT_EQ(processor()
                ->GetEntities()
                .at(storage_key)
                .specifics.history()
                .visit_duration_micros(),
            0);
}

TEST_F(HistorySyncBridgeTest, UploadsUpdatedUrlTitle) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Visit a URL.
  auto [url_row, visit_row] = AddVisitToBackendAndAdvanceClock(
      GURL("https://www.url.com"), ui::PAGE_TRANSITION_TYPED);

  // Notify the bridge about the visit - it should be sent to the processor.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row, visit_row);

  const std::string storage_key =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(
          visit_row.visit_time);
  ASSERT_EQ(processor()->GetEntities().size(), 1u);
  ASSERT_EQ(processor()->GetEntities().count(storage_key), 1u);

  // Update the URL's title.
  const std::string new_title("New title!");
  url_row.set_title(base::ASCIIToUTF16(new_title));
  ASSERT_TRUE(backend()->UpdateURL(url_row));
  bridge()->OnURLsModified(/*history_backend=*/nullptr, {url_row},
                           /*is_from_expiration=*/false);

  // The updated data should have been sent to the processor.
  EXPECT_EQ(processor()->GetEntities().size(), 1u);
  ASSERT_EQ(processor()->GetEntities().count(storage_key), 1u);
  const syncer::EntityData& entity = processor()->GetEntities().at(storage_key);
  ASSERT_EQ(entity.specifics.history().redirect_entries().size(), 1);
  EXPECT_EQ(entity.specifics.history().redirect_entries(0).title(), new_title);
}

TEST_F(HistorySyncBridgeTest, UploadsLocalVisitWithRedirects) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Create a redirect chain with 3 entries.
  URLRow url_row1(GURL("https://url1.com"));
  URLID url_id1 = backend()->AddURL(url_row1);
  url_row1.set_id(url_id1);
  URLRow url_row2(GURL("https://url2.com"));
  URLID url_id2 = backend()->AddURL(url_row2);
  url_row2.set_id(url_id2);
  URLRow url_row3(GURL("https://url3.com"));
  URLID url_id3 = backend()->AddURL(url_row3);
  url_row3.set_id(url_id3);

  // Simulate server-side redirects, which cause all visits in the chain to have
  // the same timestamp.
  const base::Time visit_time = base::Time::Now();

  VisitRow visit_row1;
  visit_row1.url_id = url_id1;
  visit_row1.visit_time = visit_time;
  visit_row1.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START);
  visit_row1.visit_id = backend()->AddVisit(visit_row1);

  VisitRow visit_row2;
  visit_row2.referring_visit = visit_row1.visit_id;
  visit_row2.url_id = url_id2;
  visit_row2.visit_time = visit_time;
  visit_row2.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_SERVER_REDIRECT);
  visit_row2.visit_id = backend()->AddVisit(visit_row2);

  VisitRow visit_row3;
  visit_row3.referring_visit = visit_row2.visit_id;
  visit_row3.url_id = url_id3;
  visit_row3.visit_time = visit_time;
  visit_row3.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_SERVER_REDIRECT |
      ui::PAGE_TRANSITION_CHAIN_END);
  visit_row3.visit_id = backend()->AddVisit(visit_row3);

  // Create content_annotations to associate with the last visit.
  VisitContentAnnotations content_annotations;
  content_annotations.has_url_keyed_image = true;
  const std::string related_search_1 = "http://www.url2.com";
  const std::string related_search_2 = "http://www.url3.com";
  content_annotations.related_searches = {related_search_1, related_search_2};
  const std::string category_id_1 = "mid1";
  const int category_weight_1 = 1;
  const std::string category_id_2 = "mid2";
  const int category_weight_2 = 2;
  content_annotations.model_annotations.categories.emplace_back(
      category_id_1, category_weight_1);
  content_annotations.model_annotations.categories.emplace_back(
      category_id_2, category_weight_2);
  backend()->AddOrReplaceContentAnnotation(visit_row3.visit_id,
                                           content_annotations);

  // Notify the bridge about all of the visits.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row1, visit_row1);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row2, visit_row2);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row3, visit_row3);

  // The whole chain should have resulted in a single entity being Put().
  const std::string storage_key =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(visit_time);
  EXPECT_EQ(processor()->GetEntities().size(), 1u);
  ASSERT_EQ(processor()->GetEntities().count(storage_key), 1u);
  const syncer::EntityData& entity = processor()->GetEntities().at(storage_key);

  // Check that the resulting entity contains the redirect chain.
  ASSERT_TRUE(entity.specifics.has_history());
  const sync_pb::HistorySpecifics& history = entity.specifics.history();
  EXPECT_EQ(base::Time::FromDeltaSinceWindowsEpoch(
                base::Microseconds(history.visit_time_windows_epoch_micros())),
            visit_time);
  EXPECT_EQ(history.originator_cache_guid(), processor()->TrackedCacheGuid());
  ASSERT_EQ(history.redirect_entries_size(), 3);
  EXPECT_EQ(history.redirect_entries(0).url(), url_row1.url());
  EXPECT_EQ(history.redirect_entries(1).url(), url_row2.url());
  EXPECT_EQ(history.redirect_entries(2).url(), url_row3.url());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      syncer::FromSyncPageTransition(
          history.page_transition().core_transition()),
      ui::PAGE_TRANSITION_LINK));
  EXPECT_TRUE(history.has_url_keyed_image());
  EXPECT_EQ(history.related_searches_size(), 2);
  EXPECT_EQ(history.related_searches(0), related_search_1);
  EXPECT_EQ(history.related_searches(1), related_search_2);
  EXPECT_EQ(history.categories_size(), 2);
  EXPECT_EQ(history.categories(0).id(), "mid1");
  EXPECT_EQ(history.categories(0).weight(), 1);
  EXPECT_EQ(history.categories(1).id(), "mid2");
  EXPECT_EQ(history.categories(1).weight(), 2);
}

TEST_F(HistorySyncBridgeTest, SplitsRedirectChainWithDifferentTimestamps) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Create a redirect chain with 2 entries.
  URLRow url_row1(GURL("https://url1.com"));
  URLID url_id1 = backend()->AddURL(url_row1);
  url_row1.set_id(url_id1);
  URLRow url_row2(GURL("https://url2.com"));
  URLID url_id2 = backend()->AddURL(url_row2);
  url_row2.set_id(url_id2);

  const base::Time visit_time_chain1 = base::Time::Now();

  VisitRow visit_row1;
  visit_row1.url_id = url_id1;
  visit_row1.visit_time = visit_time_chain1;
  visit_row1.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START);
  visit_row1.visit_id = backend()->AddVisit(visit_row1);

  VisitRow visit_row2;
  visit_row2.referring_visit = visit_row1.visit_id;
  visit_row2.url_id = url_id2;
  visit_row2.visit_time = visit_time_chain1;
  visit_row2.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_SERVER_REDIRECT |
      ui::PAGE_TRANSITION_CHAIN_END);
  visit_row2.visit_id = backend()->AddVisit(visit_row2);

  // Notify the bridge about the visits.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row1, visit_row1);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row2, visit_row2);

  // The chain should have resulted in an entity being Put().
  const std::string storage_key1 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(visit_time_chain1);
  ASSERT_EQ(processor()->GetEntities().size(), 1u);
  ASSERT_EQ(processor()->GetEntities().count(storage_key1), 1u);
  sync_pb::HistorySpecifics history1 =
      processor()->GetEntities().at(storage_key1).specifics.history();
  ASSERT_EQ(history1.redirect_entries_size(), 2);
  ASSERT_FALSE(history1.redirect_chain_start_incomplete());
  ASSERT_FALSE(history1.redirect_chain_end_incomplete());

  // Now, the chain gets extended: The last page (corresponding tovisit 2)
  // issues a client redirect (e.g. <meta http-equiv="Refresh" ...> tag).
  // First, the PAGE_TRANSITION_CHAIN_END bit gets removed from the
  // existing visit.
  visit_row2.transition = ui::PageTransitionFromInt(
      visit_row2.transition & ~ui::PAGE_TRANSITION_CHAIN_END);
  ASSERT_TRUE(backend()->UpdateVisit(visit_row2));
  // The bridge gets notified about the updated visit, but this should have no
  // effect since it's not a chain end anymore.
  bridge()->OnVisitUpdated(visit_row2, VisitUpdateReason::kUpdateTransition);

  // Two more visits get appended to the chain.
  URLRow url_row3(GURL("https://url3.com"));
  URLID url_id3 = backend()->AddURL(url_row3);
  url_row3.set_id(url_id3);
  URLRow url_row4(GURL("https://url4.com"));
  URLID url_id4 = backend()->AddURL(url_row4);
  url_row4.set_id(url_id4);

  AdvanceClock();
  const base::Time visit_time_chain2 = base::Time::Now();

  VisitRow visit_row3;
  // Link to the previous chain!
  visit_row3.referring_visit = visit_row2.visit_id;
  visit_row3.url_id = url_id3;
  visit_row3.visit_time = visit_time_chain2;
  // Note: No PAGE_TRANSITION_CHAIN_START.
  visit_row3.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  visit_row3.visit_id = backend()->AddVisit(visit_row3);

  VisitRow visit_row4;
  visit_row4.referring_visit = visit_row3.visit_id;
  visit_row4.url_id = url_id4;
  visit_row4.visit_time = visit_time_chain2;
  visit_row4.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_SERVER_REDIRECT |
      ui::PAGE_TRANSITION_CHAIN_END);
  visit_row4.visit_id = backend()->AddVisit(visit_row4);

  // Notify the bridge about the new visits.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row3, visit_row3);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row4, visit_row4);

  // Now, there should be two entities: The one from the initial chain, and a
  // separate one for the later addition.
  const std::string storage_key2 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(visit_time_chain2);
  ASSERT_EQ(processor()->GetEntities().size(), 2u);
  ASSERT_EQ(processor()->GetEntities().count(storage_key1), 1u);
  ASSERT_EQ(processor()->GetEntities().count(storage_key2), 1u);
  // The initial chain should not have the chain_end marker anymore, but be
  // otherwise unmodified.
  sync_pb::HistorySpecifics history1_expected = history1;
  history1_expected.set_redirect_chain_end_incomplete(true);
  sync_pb::HistorySpecifics history1_updated =
      processor()->GetEntities().at(storage_key1).specifics.history();
  EXPECT_EQ(syncer::HistorySpecificsToValue(history1_expected),
            syncer::HistorySpecificsToValue(history1_updated));
  // The second chain should contain only the last two entries.
  sync_pb::HistorySpecifics history2 =
      processor()->GetEntities().at(storage_key2).specifics.history();
  ASSERT_EQ(history2.redirect_entries_size(), 2);
  EXPECT_EQ(history2.redirect_entries(0).url(), url_row3.url());
  EXPECT_EQ(history2.redirect_entries(1).url(), url_row4.url());
  EXPECT_EQ(history2.originator_referring_visit_id(), visit_row2.visit_id);
  EXPECT_NE(history2.originator_cluster_id(), 0);
  EXPECT_TRUE(history2.redirect_chain_start_incomplete());
  EXPECT_FALSE(history2.redirect_chain_end_incomplete());
}

TEST_F(HistorySyncBridgeTest, DoesNotRepeatedlyUploadClientRedirects) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Visit a URL.
  URLRow url_row1(GURL("https://url1.com"));
  URLID url_id1 = backend()->AddURL(url_row1);
  url_row1.set_id(url_id1);

  const base::Time visit_time1 = base::Time::Now();

  VisitRow visit_row1;
  visit_row1.url_id = url_id1;
  visit_row1.visit_time = visit_time1;
  visit_row1.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START |
      ui::PAGE_TRANSITION_CHAIN_END);
  visit_row1.visit_id = backend()->AddVisit(visit_row1);

  // Notify the bridge about the visit.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row1, visit_row1);

  // The visit should've been Put() towards the processor.
  const std::string storage_key1 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(visit_time1);
  ASSERT_EQ(processor()->GetEntities().size(), 1u);
  ASSERT_TRUE(processor()->IsEntityUnsynced(storage_key1));

  // The entity gets uploaded to the server, and thus isn't unsynced anymore.
  processor()->MarkEntitySynced(storage_key1);

  // Now, the chain gets extended: The page issues a client redirect. First, the
  // PAGE_TRANSITION_CHAIN_END bit gets removed from the existing visit.
  visit_row1.transition = ui::PageTransitionFromInt(
      visit_row1.transition & ~ui::PAGE_TRANSITION_CHAIN_END);
  ASSERT_TRUE(backend()->UpdateVisit(visit_row1));
  // The bridge gets notified about the updated visit, but this should have no
  // effect since it's not a chain end anymore.
  bridge()->OnVisitUpdated(visit_row1, VisitUpdateReason::kUpdateTransition);

  // A visit gets appended to the chain.
  AdvanceClock();
  URLRow url_row2(GURL("https://url2.com"));
  URLID url_id2 = backend()->AddURL(url_row2);
  url_row2.set_id(url_id2);

  AdvanceClock();
  const base::Time visit_time2 = base::Time::Now();

  VisitRow visit_row2;
  // Link to the previous visit!
  visit_row2.referring_visit = visit_row1.visit_id;
  visit_row2.url_id = url_id2;
  visit_row2.visit_time = visit_time2;
  visit_row2.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT |
      ui::PAGE_TRANSITION_CHAIN_END);
  visit_row2.visit_id = backend()->AddVisit(visit_row2);

  // Notify the bridge about the new visit.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row2, visit_row2);

  // Both of the visits should've been Put() towards the processor: The first
  // one was updated, and the second one is new.
  const std::string storage_key2 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(visit_time2);
  ASSERT_EQ(processor()->GetEntities().size(), 2u);
  // The first entity should be unsynced again since it was updated.
  EXPECT_TRUE(processor()->IsEntityUnsynced(storage_key1));
  EXPECT_TRUE(processor()->IsEntityUnsynced(storage_key2));

  // They get uploaded to the server, and thus aren't unsynced anymore.
  processor()->MarkEntitySynced(storage_key1);
  processor()->MarkEntitySynced(storage_key2);

  // The chain gets extended again! First remove theCHAIN_END bit.
  visit_row2.transition = ui::PageTransitionFromInt(
      visit_row2.transition & ~ui::PAGE_TRANSITION_CHAIN_END);
  ASSERT_TRUE(backend()->UpdateVisit(visit_row2));
  bridge()->OnVisitUpdated(visit_row2, VisitUpdateReason::kUpdateTransition);

  // A visit gets appended to the chain.
  AdvanceClock();
  URLRow url_row3(GURL("https://url3.com"));
  URLID url_id3 = backend()->AddURL(url_row3);
  url_row3.set_id(url_id3);

  AdvanceClock();
  const base::Time visit_time3 = base::Time::Now();

  VisitRow visit_row3;
  // Link to the previous visit!
  visit_row3.referring_visit = visit_row2.visit_id;
  visit_row3.url_id = url_id3;
  visit_row3.visit_time = visit_time3;
  visit_row3.transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT |
      ui::PAGE_TRANSITION_CHAIN_END);
  visit_row3.visit_id = backend()->AddVisit(visit_row3);

  // Notify the bridge about the new visit.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row3, visit_row3);

  // The last *two* visits should've been Put() to the processor: The second was
  // updated, and the third is new.
  const std::string storage_key3 =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(visit_time3);
  ASSERT_EQ(processor()->GetEntities().size(), 3u);
  // This is the main expectation of the test: The first visit was not changed,
  // so it should *not* be unsynced again.
  EXPECT_FALSE(processor()->IsEntityUnsynced(storage_key1));
  EXPECT_TRUE(processor()->IsEntityUnsynced(storage_key2));
  EXPECT_TRUE(processor()->IsEntityUnsynced(storage_key3));
}

TEST_F(HistorySyncBridgeTest, TrimsExcessivelyLongRedirectChain) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Create a redirect chain with many entries.
  constexpr int kNumRedirects = 100;
  const base::Time visit_time = base::Time::Now();
  VisitID previous_visit = kInvalidVisitID;
  for (int i = 1; i <= kNumRedirects; i++) {
    URLRow url_row(GetURL(i));
    url_row.set_id(backend()->AddURL(url_row));

    VisitRow visit_row;
    visit_row.url_id = url_row.id();
    visit_row.visit_time = visit_time;
    visit_row.referring_visit = previous_visit;
    int transition = ui::PAGE_TRANSITION_LINK;
    if (i > 0) {
      transition |= ui::PAGE_TRANSITION_SERVER_REDIRECT;
    }
    if (i == 1) {
      transition |= ui::PAGE_TRANSITION_CHAIN_START;
    }
    if (i == kNumRedirects) {
      transition |= ui::PAGE_TRANSITION_CHAIN_END;
    }
    visit_row.transition = ui::PageTransitionFromInt(transition);
    previous_visit = visit_row.visit_id = backend()->AddVisit(visit_row);

    bridge()->OnURLVisited(
        /*history_backend=*/nullptr, url_row, visit_row);
  }

  // The chain should have been Put() to the processor.
  ASSERT_EQ(processor()->GetEntities().size(), 1u);
  const std::string storage_key =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(visit_time);
  ASSERT_EQ(processor()->GetEntities().count(storage_key), 1u);
  // ...but since it is excessively long, it should have been trimmed.
  sync_pb::HistorySpecifics history =
      processor()->GetEntities().at(storage_key).specifics.history();
  EXPECT_LT(history.redirect_entries_size(), kNumRedirects);
  // The entity should also be flagged as "trimmed".
  EXPECT_TRUE(history.redirect_chain_middle_trimmed());
  EXPECT_FALSE(history.redirect_chain_start_incomplete());
  EXPECT_FALSE(history.redirect_chain_end_incomplete());
  // At least the first and the last entry should have survived.
  ASSERT_GE(history.redirect_entries_size(), 2);
  EXPECT_EQ(GURL(history.redirect_entries(0).url()), GetURL(1));
  EXPECT_EQ(
      GURL(history.redirect_entries(history.redirect_entries_size() - 1).url()),
      GetURL(kNumRedirects));
}

TEST_F(HistorySyncBridgeTest, DownloadsUpdatedEntity) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // A remote visit comes in.
  sync_pb::HistorySpecifics remote_specifics =
      CreateSpecifics(base::Time::Now() - base::Seconds(5), "remote_cache_guid",
                      GURL("https://remote.com"));
  ApplyIncrementalSyncChanges({remote_specifics});

  // Make sure it has neither a URL title nor a visit duration.
  ASSERT_EQ(backend()->GetURLs().size(), 1u);
  ASSERT_TRUE(backend()->GetURLs()[0].title().empty());
  ASSERT_EQ(backend()->GetVisits().size(), 1u);
  ASSERT_EQ(backend()->GetVisits()[0].visit_duration, base::TimeDelta());

  // The remote visit gets updated with a URL title and visit duration.
  remote_specifics.mutable_redirect_entries(0)->set_title("Title");
  remote_specifics.set_visit_duration_micros(1234);
  ApplyIncrementalSyncChanges({remote_specifics});

  // Make sure these changes arrived in the backend.
  ASSERT_EQ(backend()->GetURLs().size(), 1u);
  EXPECT_EQ(backend()->GetURLs()[0].title(), u"Title");
  ASSERT_EQ(backend()->GetVisits().size(), 1u);
  EXPECT_EQ(backend()->GetVisits()[0].visit_duration, base::Microseconds(1234));
}

TEST_F(HistorySyncBridgeTest, UntracksEntitiesAfterCommit) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Visit some URLs.
  auto [url_row1, visit_row1] = AddVisitToBackendAndAdvanceClock(
      GURL("https://url1.com"), ui::PAGE_TRANSITION_TYPED);
  auto [url_row2, visit_row2] = AddVisitToBackendAndAdvanceClock(
      GURL("https://url2.com"), ui::PAGE_TRANSITION_LINK);

  // Notify the bridge about the visits - they should be sent to the processor.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row1, visit_row1);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row2, visit_row2);

  EXPECT_EQ(processor()->GetEntities().size(), 2u);
  // The metadata for these entities should now be tracked.
  EXPECT_EQ(GetPersistedEntityMetadata().size(), 2u);

  // Simulate a successful commit, which results in an
  // ApplyIncrementalSyncChanges() call to the bridge, updating the committed
  // entities' metadata.
  std::vector<std::string> updated_storage_keys;
  for (const auto& [storage_key, metadata] : GetPersistedEntityMetadata()) {
    processor()->MarkEntitySynced(storage_key);
    updated_storage_keys.push_back(storage_key);
  }
  ApplyIncrementalSyncChanges({}, updated_storage_keys);

  // Now the metadata should not be tracked anymore.
  EXPECT_TRUE(GetPersistedEntityMetadata().empty());
}

TEST_F(HistorySyncBridgeTest, UntracksRemoteEntities) {
  // Start Sync with an initial remote entity.
  ApplyInitialSyncChanges(
      {CreateSpecifics(base::Time::Now() - base::Seconds(10),
                       "remote_cache_guid", GURL("https://remote.com"))});
  ASSERT_EQ(backend()->GetURLs().size(), 1u);
  ASSERT_EQ(backend()->GetVisits().size(), 1u);
  ASSERT_EQ(backend()->GetVisits()[0].visit_duration, base::TimeDelta());

  // The entity should have been untracked immediately.
  EXPECT_TRUE(GetPersistedEntityMetadata().empty());

  // Another remote entity comes in.
  ApplyIncrementalSyncChanges(
      {CreateSpecifics(base::Time::Now() - base::Seconds(5),
                       "remote_cache_guid", GURL("https://remote2.com"))});

  // This entity should also have been untracked immediately.
  EXPECT_TRUE(GetPersistedEntityMetadata().empty());
}

TEST_F(HistorySyncBridgeTest, DoesNotUntrackEntityPendingCommit) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Visit a URL locally.
  auto [url_row1, visit_row1] = AddVisitToBackendAndAdvanceClock(
      GURL("https://url1.com"), ui::PAGE_TRANSITION_TYPED);

  const std::string storage_key =
      HistorySyncMetadataDatabase::StorageKeyFromVisitTime(
          visit_row1.visit_time);

  // Notify the bridge about the visit.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row1, visit_row1);

  EXPECT_EQ(processor()->GetEntities().size(), 1u);

  // The metadata for this entity should now be tracked.
  ASSERT_EQ(GetPersistedEntityMetadata().size(), 1u);

  // Before the entity gets committed (and thus untracked), a remote entity
  // comes in.
  ApplyIncrementalSyncChanges({CreateSpecifics(
      base::Time::Now(), "remote_cache_guid", GURL("https://remote.com"))});

  // The remote entity should have been untracked immediately, but the local
  // entity pending commit should still be tracked.
  syncer::EntityMetadataMap metadata = GetPersistedEntityMetadata();
  EXPECT_EQ(metadata.size(), 1u);
  EXPECT_EQ(metadata.count(storage_key), 1u);
}

TEST_F(HistorySyncBridgeTest, UntracksEntityOnIndividualDeletion) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Visit some URLs.
  auto [url_row1, visit_row1] = AddVisitToBackendAndAdvanceClock(
      GURL("https://url1.com"), ui::PAGE_TRANSITION_TYPED);
  auto [url_row2, visit_row2] = AddVisitToBackendAndAdvanceClock(
      GURL("https://url2.com"), ui::PAGE_TRANSITION_LINK);

  // Notify the bridge about the visits - they should be sent to the processor.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row1, visit_row1);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row2, visit_row2);
  ASSERT_EQ(GetPersistedEntityMetadata().size(), 2u);

  EXPECT_EQ(processor()->GetEntities().size(), 2u);

  // Now, *before* the entities get committed successfully (and thus would get
  // untracked anyway), delete the first URL+visit and notify the bridge. This
  // should not result in any Put() or Delete() calls to the processor
  // (deletions are handled through the separate HISTORY_DELETE_DIRECTIVES data
  // type), but it should untrack the deleted entity.
  backend()->RemoveURLAndVisits(url_row1.id());

  bridge()->OnVisitDeleted(visit_row1);
  bridge()->OnHistoryDeletions(
      /*history_backend=*/nullptr, /*all_history=*/false,
      /*expired=*/false, {url_row1}, /*favicon_urls=*/{});
  // The metadata for the first (deleted) entity should be gone, but the
  // metadata for the second entity should still exist.
  EXPECT_EQ(GetPersistedEntityMetadata().size(), 1u);
}

TEST_F(HistorySyncBridgeTest,
       UntracksEntityOnIndividualDeletionWhileSyncPaused) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Visit some URLs.
  auto [url_row1, visit_row1] = AddVisitToBackendAndAdvanceClock(
      GURL("https://url1.com"), ui::PAGE_TRANSITION_TYPED);
  auto [url_row2, visit_row2] = AddVisitToBackendAndAdvanceClock(
      GURL("https://url2.com"), ui::PAGE_TRANSITION_LINK);

  // Notify the bridge about the visits - they should be sent to the processor.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row1, visit_row1);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row2, visit_row2);
  ASSERT_EQ(GetPersistedEntityMetadata().size(), 2u);

  EXPECT_EQ(processor()->GetEntities().size(), 2u);

  // Sync gets paused. In this state, the bridge will not send any more data to
  // the processor, but deletions should still cause entities to get untracked.
  bridge()->SetSyncTransportState(syncer::SyncService::TransportState::PAUSED);
  bridge()->OnSyncPaused();  // No-op, but for the sake of a realistic sequence.

  // While in the Sync-paused state (and before the entities get committed
  // successfully and thus would get untracked anyway), delete the first
  // URL+visit and notify the bridge. This should not result in any Put() or
  // Delete() calls to the processor (deletions are handled through the separate
  // HISTORY_DELETE_DIRECTIVES data type), but it should untrack the deleted
  // entity.
  backend()->RemoveURLAndVisits(url_row1.id());

  bridge()->OnVisitDeleted(visit_row1);
  bridge()->OnHistoryDeletions(
      /*history_backend=*/nullptr, /*all_history=*/false,
      /*expired=*/false, {url_row1}, /*favicon_urls=*/{});
  // The metadata for the first (deleted) entity should be gone, but the
  // metadata for the second entity should still exist.
  EXPECT_EQ(GetPersistedEntityMetadata().size(), 1u);
}

TEST_F(HistorySyncBridgeTest, UntracksAllEntitiesOnAllHistoryDeletion) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Add some visits to the DB.
  auto [url_row1, visit_row1] = AddVisitToBackendAndAdvanceClock(
      GURL("https://url1.com"), ui::PAGE_TRANSITION_TYPED);
  auto [url_row2, visit_row2] = AddVisitToBackendAndAdvanceClock(
      GURL("https://url2.com"), ui::PAGE_TRANSITION_LINK);

  // Notify the bridge about the visits - they should be sent to the processor.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row1, visit_row1);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row2, visit_row2);
  ASSERT_EQ(GetPersistedEntityMetadata().size(), 2u);

  EXPECT_EQ(processor()->GetEntities().size(), 2u);

  // Now, *before* the entities get committed successfully (and thus would get
  // untracked anyway), simulate a delete-all-history operation. This should not
  // result in any Put() or Delete() calls on the processor (deletions are
  // handled through the separate HISTORY_DELETE_DIRECTIVES data type), but it
  // should untrack all entities.
  backend()->Clear();
  // Deleting all history does *not* result in OnVisitDeleted() calls, and also
  // does not include the actual deleted URLs in OnURLsDeleted().
  bridge()->OnHistoryDeletions(/*history_backend=*/nullptr,
                               /*all_history=*/true,
                               /*expired=*/false, /*deleted_rows=*/{},
                               /*favicon_urls=*/{});

  EXPECT_TRUE(GetPersistedEntityMetadata().empty());
}

TEST_F(HistorySyncBridgeTest,
       UntracksAllEntitiesOnAllHistoryDeletionWhileSyncPaused) {
  // Start syncing (with no data yet).
  ApplyInitialSyncChanges({});

  // Add some visits to the DB.
  auto [url_row1, visit_row1] = AddVisitToBackendAndAdvanceClock(
      GURL("https://url1.com"), ui::PAGE_TRANSITION_TYPED);
  auto [url_row2, visit_row2] = AddVisitToBackendAndAdvanceClock(
      GURL("https://url2.com"), ui::PAGE_TRANSITION_LINK);

  // Notify the bridge about the visits - they should be sent to the processor.
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row1, visit_row1);
  bridge()->OnURLVisited(
      /*history_backend=*/nullptr, url_row2, visit_row2);
  ASSERT_EQ(GetPersistedEntityMetadata().size(), 2u);

  EXPECT_EQ(processor()->GetEntities().size(), 2u);

  // Sync gets paused. In this state, the bridge will not send any more data to
  // the processor, but deletions should still cause entities to get untracked.
  bridge()->SetSyncTransportState(syncer::SyncService::TransportState::PAUSED);
  bridge()->OnSyncPaused();  // No-op, but for the sake of a realistic sequence.

  // While in the Sync-paused state (and before the entities get committed
  // successfully and thus would get untracked anyway), simulate a
  // delete-all-history operation. This should not result in any Put() or
  // Delete() calls to the processor (deletions are handled through the separate
  // HISTORY_DELETE_DIRECTIVES data type), but it should untrack the deleted
  // entity.
  backend()->Clear();
  // Deleting all history does *not* result in OnVisitDeleted() calls, and also
  // does not include the actual deleted URLs in OnURLsDeleted().
  bridge()->OnHistoryDeletions(/*history_backend=*/nullptr,
                               /*all_history=*/true,
                               /*expired=*/false, /*deleted_rows=*/{},
                               /*favicon_urls=*/{});

  EXPECT_TRUE(GetPersistedEntityMetadata().empty());
}

// Note: The remapping logic is covered in the separate test suite
// VisitIDRemapperTest. This test serves as an "integration test" for the
// plumbing from HistorySyncBridge to VisitIDRemapper.
TEST_F(HistorySyncBridgeTest, RemapsOriginatorVisitIDs) {
  const std::string remote_cache_guid("remote_cache_guid");

  // Situation: There's a first visit, which refers to a chain of 3 visits,
  // which in turn opens a last visit.

  const base::Time first_visit_time = base::Time::Now() - base::Minutes(10);
  const VisitID first_visit_originator_id = 100;
  sync_pb::HistorySpecifics entity_first =
      CreateSpecifics(first_visit_time, remote_cache_guid,
                      GURL("https://some.url"), first_visit_originator_id);

  const base::Time chain_visit_time = base::Time::Now() - base::Minutes(9);
  const std::vector<GURL> chain_urls{GURL("https://start.chain.url"),
                                     GURL("https://middle.chain.url"),
                                     GURL("https://end.chain.url")};
  const std::vector<VisitID> chain_originator_visit_ids{101, 102, 103};
  sync_pb::HistorySpecifics entity_chain =
      CreateSpecifics(chain_visit_time, remote_cache_guid, chain_urls,
                      chain_originator_visit_ids);
  entity_chain.set_originator_referring_visit_id(
      entity_first.redirect_entries(0).originator_visit_id());

  const base::Time last_visit_time = base::Time::Now() - base::Minutes(8);
  const VisitID last_visit_originator_id = 104;
  sync_pb::HistorySpecifics entity_last =
      CreateSpecifics(last_visit_time, remote_cache_guid,
                      GURL("https://other.url"), last_visit_originator_id);
  entity_last.set_originator_opener_visit_id(
      entity_chain.redirect_entries(2).originator_visit_id());

  // Start syncing with these three entities - this should trigger the remapping
  // of originator IDs into local IDs.
  ApplyInitialSyncChanges({entity_first, entity_chain, entity_last});

  VisitRow first_row;
  ASSERT_TRUE(backend()->GetLastVisitByTime(first_visit_time, &first_row));
  ASSERT_EQ(first_row.originator_visit_id, first_visit_originator_id);

  VisitRow chain_end_row;
  ASSERT_TRUE(backend()->GetLastVisitByTime(chain_visit_time, &chain_end_row));
  ASSERT_EQ(chain_end_row.originator_visit_id,
            chain_originator_visit_ids.back());
  // Make sure the chain got preserved (note that GetRedirectChain is based on
  // *local* visit IDs, not originator IDs).
  VisitVector chain_rows = backend()->GetRedirectChain(chain_end_row);
  EXPECT_EQ(chain_rows.size(), 3u);
  // Make sure the referrer (first visit) got remapped.
  EXPECT_EQ(chain_rows.front().referring_visit, first_row.visit_id);

  VisitRow last_row;
  ASSERT_TRUE(backend()->GetLastVisitByTime(last_visit_time, &last_row));
  ASSERT_EQ(last_row.originator_visit_id, last_visit_originator_id);
  // Make sure the opener (last visit of the chain) got remapped.
  EXPECT_EQ(last_row.opener_visit, chain_rows.back().visit_id);
  // No originator cluster id provided.
  EXPECT_EQ(backend()->add_visit_to_synced_cluster_count(), 0);
}

TEST_F(HistorySyncBridgeTest, RemapsLegacyRedirectChain) {
  const std::string remote_cache_guid("remote_cache_guid");

  // Situation: There's a redirect chain of 3 visits, coming from a legacy
  // client, meaning that their originator visit IDs are unset (zero).

  const base::Time visit_time = base::Time::Now() - base::Minutes(9);
  const std::vector<GURL> urls{GURL("https://start.chain.url"),
                               GURL("https://middle.chain.url"),
                               GURL("https://end.chain.url")};
  const std::vector<VisitID> originator_visit_ids{0, 0, 0};
  sync_pb::HistorySpecifics entity = CreateSpecifics(
      visit_time, remote_cache_guid, urls, originator_visit_ids);

  // Start syncing - this should trigger the creation of local referrer IDs.
  ApplyInitialSyncChanges({entity});

  VisitRow chain_end_row;
  ASSERT_TRUE(backend()->GetLastVisitByTime(visit_time, &chain_end_row));
  // Make sure the chain got preserved (even though there were no originator
  // visit IDs, and thus no explicit links between the individual visits).
  VisitVector chain_rows = backend()->GetRedirectChain(chain_end_row);
  EXPECT_EQ(chain_rows.size(), 3u);
}

TEST_F(HistorySyncBridgeTest, AddsCluster) {
  const std::string remote_cache_guid("remote_cache_guid");

  const base::Time visit_time = base::Time::Now() - base::Minutes(9);
  const std::vector<GURL> urls{GURL("https://start.chain.url"),
                               GURL("https://middle.chain.url"),
                               GURL("https://end.chain.url")};
  const std::vector<VisitID> originator_visit_ids{0, 0, 0};
  sync_pb::HistorySpecifics entity = CreateSpecifics(
      visit_time, remote_cache_guid, urls, originator_visit_ids);
  entity.set_originator_cluster_id(1);

  // Start syncing - this should trigger the creation of local cluster IDs.
  ApplyInitialSyncChanges({entity});

  VisitRow chain_end_row;
  ASSERT_TRUE(backend()->GetLastVisitByTime(visit_time, &chain_end_row));
  // Make sure the chain got preserved (even though there were no originator
  // visit IDs, and thus no explicit links between the individual visits).
  VisitVector chain_rows = backend()->GetRedirectChain(chain_end_row);
  EXPECT_EQ(chain_rows.size(), 3u);

  // Should be called once per visit.
  EXPECT_EQ(backend()->add_visit_to_synced_cluster_count(), 3);
}

}  // namespace

}  // namespace history
