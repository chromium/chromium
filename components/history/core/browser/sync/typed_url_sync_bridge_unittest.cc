// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/typed_url_sync_bridge.h"

#include <algorithm>
#include <memory>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/test/test_history_database.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/recording_model_type_change_processor.h"
#include "components/sync/model/sync_metadata_store.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using sync_pb::TypedUrlSpecifics;
using syncer::DataBatch;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::KeyAndData;
using syncer::MetadataBatch;
using syncer::MetadataChangeList;
using syncer::RecordingModelTypeChangeProcessor;

namespace history {

namespace {

// Constants used to limit size of visits processed. See
// equivalent constants in typed_url_sync_bridge.cc for descriptions.
const int kMaxTypedUrlVisits = 100;
const int kVisitThrottleThreshold = 10;
const int kVisitThrottleMultiple = 10;

// Visits with this timestamp are treated as expired.
const int kExpiredVisit = -1;

// Helper constants for tests.
const char kTitle[] = "pie";
const char kTitle2[] = "cookie";
const char kURL[] = "http://pie.com/";
const char kURL2[] = "http://cookie.com/";

Time SinceEpoch(int64_t microseconds_since_epoch) {
  return Time::FromDeltaSinceWindowsEpoch(
      TimeDelta::FromMicroseconds(microseconds_since_epoch));
}

bool URLsEqual(const URLRow& row, const sync_pb::TypedUrlSpecifics& specifics) {
  return ((row.url().spec().compare(specifics.url()) == 0) &&
          (base::UTF16ToUTF8(row.title()).compare(specifics.title()) == 0) &&
          (row.hidden() == specifics.hidden()));
}

bool URLsEqual(const URLRow& lhs, const URLRow& rhs) {
  // Only compare synced fields (ignore typed_count and visit_count as those
  // are maintained by the history subsystem).
  return (lhs.url().spec().compare(rhs.url().spec()) == 0) &&
         (lhs.title().compare(rhs.title()) == 0) &&
         (lhs.hidden() == rhs.hidden());
}

void AddNewestVisit(ui::PageTransition transition,
                    int64_t visit_time,
                    URLRow* url,
                    VisitVector* visits) {
  Time time = SinceEpoch(visit_time);
  visits->insert(visits->begin(),
                 VisitRow(url->id(), time, 0, transition, 0,
                          HistoryBackend::IsTypedIncrement(transition)));

  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED)) {
    url->set_typed_count(url->typed_count() + 1);
  }

  url->set_last_visit(time);
  url->set_visit_count(visits->size());
}

void AddOldestVisit(ui::PageTransition transition,
                    int visit_time,
                    URLRow* url,
                    VisitVector* visits) {
  Time time = SinceEpoch(visit_time);
  visits->push_back(VisitRow(url->id(), time, 0, transition, 0,
                             HistoryBackend::IsTypedIncrement(transition)));

  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED)) {
    url->set_typed_count(url->typed_count() + 1);
  }

  url->set_visit_count(visits->size());
}

// Create a new row object and the typed visit çorresponding with the time at
// |last_visit| in the |visits| vector.
URLRow MakeTypedUrlRow(const std::string& url,
                       const std::string& title,
                       int typed_count,
                       int64_t last_visit,
                       bool hidden,
                       VisitVector* visits) {
  // Give each URL a unique ID, to mimic the behavior of the real database.
  GURL gurl(url);
  URLRow history_url(gurl);
  history_url.set_title(base::UTF8ToUTF16(title));
  history_url.set_typed_count(typed_count);
  history_url.set_hidden(hidden);

  Time last_visit_time = SinceEpoch(last_visit);
  history_url.set_last_visit(last_visit_time);

  if (typed_count > 0) {
    // Add a typed visit for time |last_visit_time|.
    visits->push_back(VisitRow(history_url.id(), last_visit_time, 0,
                               ui::PAGE_TRANSITION_TYPED, 0, true));
  } else {
    // Add a non-typed visit for time |last_visit_time|.
    visits->push_back(VisitRow(history_url.id(), last_visit_time, 0,
                               ui::PAGE_TRANSITION_RELOAD, 0, false));
  }

  history_url.set_visit_count(visits->size());
  return history_url;
}

// Create a new row object and a typed and a reload visit with appropriate
// times.
URLRow MakeTypedUrlRowWithTwoVisits(const std::string& url,
                                    const std::string& title,
                                    int64_t typed_visit,
                                    int64_t reload_visit,
                                    bool hidden,
                                    VisitVector* visits) {
  // Give each URL a unique ID, to mimic the behavior of the real database.
  GURL gurl(url);
  URLRow history_url(gurl);
  history_url.set_title(base::UTF8ToUTF16(title));
  history_url.set_typed_count(1);
  history_url.set_visit_count(2);
  history_url.set_hidden(hidden);

  Time typed_visit_time = SinceEpoch(typed_visit);
  Time reload_visit_time = SinceEpoch(reload_visit);

  history_url.set_last_visit(std::max(typed_visit_time, reload_visit_time));

  visits->push_back(VisitRow(history_url.id(), typed_visit_time, 0,
                             ui::PAGE_TRANSITION_TYPED, 0, true));
  // Add a non-typed visit for time |last_visit|.
  visits->push_back(VisitRow(history_url.id(), reload_visit_time, 0,
                             ui::PAGE_TRANSITION_RELOAD, 0, false));
  return history_url;
}

void VerifyEqual(const TypedUrlSpecifics& s1, const TypedUrlSpecifics& s2) {
  // Instead of just comparing serialized strings, manually check fields to show
  // differences on failure.
  EXPECT_EQ(s1.url(), s2.url());
  EXPECT_EQ(s1.title(), s2.title());
  EXPECT_EQ(s1.hidden(), s2.hidden());
  EXPECT_EQ(s1.visits_size(), s2.visits_size());
  EXPECT_EQ(s1.visit_transitions_size(), s2.visit_transitions_size());
  EXPECT_EQ(s1.visits_size(), s1.visit_transitions_size());
  int size = s1.visits_size();
  for (int i = 0; i < size; ++i) {
    EXPECT_EQ(s1.visits(i), s2.visits(i)) << "visits differ at index " << i;
    EXPECT_EQ(s1.visit_transitions(i), s2.visit_transitions(i))
        << "visit_transitions differ at index " << i;
  }
}

void VerifyDataBatch(std::map<std::string, TypedUrlSpecifics> expected,
                     std::unique_ptr<DataBatch> batch) {
  while (batch->HasNext()) {
    const KeyAndData& pair = batch->Next();
    auto iter = expected.find(pair.first);
    ASSERT_NE(iter, expected.end());
    VerifyEqual(iter->second, pair.second->specifics.typed_url());
    // Removing allows us to verify we don't see the same item multiple times,
    // and that we saw everything we expected.
    expected.erase(iter);
  }
  EXPECT_TRUE(expected.empty());
}

std::string IntToStroageKey(int id) {
  std::string storage_key(sizeof(URLID), 0);
  base::WriteBigEndian<URLID>(&storage_key[0], id);
  return storage_key;
}

class TestHistoryBackendDelegate : public HistoryBackend::Delegate {
 public:
  TestHistoryBackendDelegate() {}

  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override {}
  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override {}
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {}
  void NotifyURLVisited(ui::PageTransition transition,
                        const URLRow& row,
                        const RedirectList& redirects,
                        Time visit_time) override {}
  void NotifyURLsModified(const URLRows& changed_urls) override {}
  void NotifyURLsDeleted(DeletionInfo deletion_info) override {}
  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const base::string16& term) override {}
  void NotifyKeywordSearchTermDeleted(URLID url_id) override {}
  void DBLoaded() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestHistoryBackendDelegate);
};

class TestHistoryBackend : public HistoryBackend {
 public:
  TestHistoryBackend()
      : HistoryBackend(std::make_unique<TestHistoryBackendDelegate>(),
                       nullptr,
                       base::ThreadTaskRunnerHandle::Get()) {}

  bool IsExpiredVisitTime(const Time& time) override {
    return time.ToDeltaSinceWindowsEpoch().InMicroseconds() == kExpiredVisit;
  }

  URLID GetIdByUrl(const GURL& gurl) {
    return db()->GetRowForURL(gurl, nullptr);
  }

  void SetVisitsForUrl(URLRow* new_url, const VisitVector visits) {
    if (!GetURL(new_url->url(), nullptr)) {
      URLRows new_urls;
      new_urls.push_back(*new_url);
      AddPagesWithDetails(new_urls, SOURCE_SYNCED);
    }

    std::vector<VisitInfo> added_visits;
    for (const auto& visit : visits) {
      added_visits.push_back(VisitInfo(visit.visit_time, visit.transition));
    }
    AddVisits(new_url->url(), added_visits, SOURCE_SYNCED);
    new_url->set_id(GetIdByUrl(new_url->url()));
  }

 private:
  ~TestHistoryBackend() override {}
};

}  // namespace

class TypedURLSyncBridgeTest : public testing::Test {
 public:
  TypedURLSyncBridgeTest() : typed_url_sync_bridge_(nullptr) {}
  ~TypedURLSyncBridgeTest() override {}

  void SetUp() override {
    fake_history_backend_ = new TestHistoryBackend();
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    fake_history_backend_->Init(
        false, TestHistoryDatabaseParamsForPath(test_dir_.GetPath()));
    std::unique_ptr<TypedURLSyncBridge> bridge = std::make_unique<
        TypedURLSyncBridge>(
        fake_history_backend_.get(), fake_history_backend_->db(),
        RecordingModelTypeChangeProcessor::CreateProcessorAndAssignRawPointer(
            &processor_));
    typed_url_sync_bridge_ = bridge.get();
    typed_url_sync_bridge_->Init();
    typed_url_sync_bridge_->history_backend_observer_.RemoveAll();
    fake_history_backend_->SetTypedURLSyncBridgeForTest(std::move(bridge));
  }

  void TearDown() override {
    VerifyProcessorReceivedValidEntityData();
    fake_history_backend_->Closing();
  }

  // Starts sync for |typed_url_sync_bridge_| with |initial_data| as the
  // initial sync data.
  void StartSyncing(const std::vector<TypedUrlSpecifics>& specifics) {
    // Set change processor.
    const auto error =
        bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(),
                                CreateEntityChangeList(specifics));

    EXPECT_FALSE(error);
  }

  bool BuildAndPushLocalChanges(unsigned int num_typed_urls,
                                unsigned int num_reload_urls,
                                const std::vector<std::string>& urls,
                                URLRows* rows,
                                std::vector<VisitVector>* visit_vectors) {
    unsigned int total_urls = num_typed_urls + num_reload_urls;
    DCHECK(urls.size() >= total_urls);
    if (!bridge())
      return false;

    if (total_urls) {
      // Create new URL rows, populate the mock backend with its visits, and
      // send to the sync service.
      URLRows changed_urls;

      for (unsigned int i = 0; i < total_urls; ++i) {
        int typed = i < num_typed_urls ? 1 : 0;
        VisitVector visits;
        visit_vectors->push_back(visits);
        rows->push_back(MakeTypedUrlRow(urls[i], kTitle, typed, i + 3, false,
                                        &visit_vectors->back()));
        fake_history_backend_->SetVisitsForUrl(&rows->back(),
                                               visit_vectors->back());
        changed_urls.push_back(rows->back());
      }

      bridge()->OnURLsModified(fake_history_backend_.get(), changed_urls,
                               /*is_from_expiration=*/false);
    }

    // Check that communication with sync was successful.
    if (num_typed_urls != processor().put_multimap().size())
      return false;
    return true;
  }

  VisitVector ApplyUrlAndVisitsChange(const std::string& url,
                                      const std::string& title,
                                      int typed_count,
                                      int64_t last_visit,
                                      bool hidden,
                                      bool update_metadata,
                                      EntityChange::ChangeType change_type) {
    VisitVector visits;
    URLRow row =
        MakeTypedUrlRow(url, title, typed_count, last_visit, hidden, &visits);
    sync_pb::TypedUrlSpecifics typed_url_specifics;
    WriteToTypedUrlSpecifics(row, visits, &typed_url_specifics);
    std::string storage_key = GetStorageKey(typed_url_specifics.url());

    EntityChangeList entity_changes;
    switch (change_type) {
      case EntityChange::ACTION_ADD:
        entity_changes.push_back(EntityChange::CreateAdd(
            std::string(), SpecificsToEntity(typed_url_specifics)));
        break;
      case EntityChange::ACTION_UPDATE:
        entity_changes.push_back(EntityChange::CreateUpdate(
            storage_key, SpecificsToEntity(typed_url_specifics)));
        break;
      case EntityChange::ACTION_DELETE:
        entity_changes.push_back(EntityChange::CreateDelete(storage_key));
        break;
    }

    std::unique_ptr<MetadataChangeList> metadata_changes =
        bridge()->CreateMetadataChangeList();
    // There needs to be data present in the history DB before inserting
    // metadata.
    if (update_metadata) {
      sync_pb::EntityMetadata metadata;
      metadata.set_sequence_number(1);
      metadata_changes->UpdateMetadata(storage_key, metadata);
    }

    bridge()->ApplySyncChanges(std::move(metadata_changes),
                               std::move(entity_changes));
    return visits;
  }

  void AddObserver() {
    bridge()->history_backend_observer_.Add(fake_history_backend_.get());
  }

  void RemoveObserver() { bridge()->history_backend_observer_.RemoveAll(); }

  // Fills |specifics| with the sync data for |url| and |visits|.
  static bool WriteToTypedUrlSpecifics(const URLRow& url,
                                       const VisitVector& visits,
                                       TypedUrlSpecifics* specifics) {
    return TypedURLSyncBridge::WriteToTypedUrlSpecifics(url, visits, specifics);
  }

  std::string GetStorageKey(const std::string& url) {
    return bridge()->GetStorageKeyInternal(url);
  }

  std::unique_ptr<EntityData> SpecificsToEntity(
      const TypedUrlSpecifics& specifics) {
    auto data = std::make_unique<EntityData>();
    *data->specifics.mutable_typed_url() = specifics;
    return data;
  }

  EntityChangeList CreateEntityChangeList(
      const std::vector<TypedUrlSpecifics>& specifics_vector) {
    EntityChangeList entity_change_list;
    for (const auto& specifics : specifics_vector) {
      entity_change_list.push_back(EntityChange::CreateAdd(
          GetStorageKey(specifics.url()), SpecificsToEntity(specifics)));
    }
    return entity_change_list;
  }

  std::map<std::string, TypedUrlSpecifics> ExpectedMap(
      const std::vector<TypedUrlSpecifics>& specifics_vector) {
    std::map<std::string, TypedUrlSpecifics> map;
    for (const auto& specifics : specifics_vector) {
      map[GetStorageKey(specifics.url())] = specifics;
    }
    return map;
  }

  void VerifyAllLocalHistoryData(
      const std::vector<TypedUrlSpecifics>& expected) {
    bridge()->GetAllDataForDebugging(
        base::BindOnce(&VerifyDataBatch, ExpectedMap(expected)));
  }

  void VerifyGetData(TypedURLSyncBridge::StorageKeyList storage_keys,
                     const std::vector<TypedUrlSpecifics>& expected) {
    bridge()->GetData(storage_keys,
                      base::BindOnce(&VerifyDataBatch, ExpectedMap(expected)));
  }

  void VerifyProcessorReceivedValidEntityData() {
    for (const auto& it : processor().put_multimap()) {
      EXPECT_GT(TypedURLSyncMetadataDatabase::StorageKeyToURLID(it.first), 0);
      EXPECT_TRUE(it.second->specifics.has_typed_url());
    }
  }

  sync_pb::TypedUrlSpecifics GetLastUpdateForURL(const std::string& url) {
    const std::string storage_key = GetStorageKey(url);
    auto eq_range = processor().put_multimap().equal_range(storage_key);
    if (eq_range.first == eq_range.second)
      return sync_pb::TypedUrlSpecifics();

    auto recorded_specifics_iterator = --eq_range.second;
    EXPECT_NE(processor().put_multimap().end(), recorded_specifics_iterator);
    EXPECT_TRUE(recorded_specifics_iterator->second->specifics.has_typed_url());

    return recorded_specifics_iterator->second->specifics.typed_url();
  }

  static void DiffVisits(const VisitVector& history_visits,
                         const sync_pb::TypedUrlSpecifics& sync_specifics,
                         std::vector<VisitInfo>* new_visits,
                         VisitVector* removed_visits) {
    TypedURLSyncBridge::DiffVisits(history_visits, sync_specifics, new_visits,
                                   removed_visits);
  }

  static VisitRow CreateVisit(ui::PageTransition type, int64_t timestamp) {
    return VisitRow(0, SinceEpoch(timestamp), 0, type, 0,
                    HistoryBackend::IsTypedIncrement(type));
  }

  static TypedURLSyncBridge::MergeResult MergeUrls(
      const sync_pb::TypedUrlSpecifics& typed_url,
      const URLRow& url,
      VisitVector* visits,
      URLRow* new_url,
      std::vector<VisitInfo>* new_visits) {
    return TypedURLSyncBridge::MergeUrls(typed_url, url, visits, new_url,
                                         new_visits);
  }

  static sync_pb::TypedUrlSpecifics MakeTypedUrlSpecifics(const char* url,
                                                          const char* title,
                                                          int64_t last_visit,
                                                          bool hidden) {
    sync_pb::TypedUrlSpecifics typed_url;
    typed_url.set_url(url);
    typed_url.set_title(title);
    typed_url.set_hidden(hidden);
    typed_url.add_visits(last_visit);
    typed_url.add_visit_transitions(ui::PAGE_TRANSITION_TYPED);
    return typed_url;
  }

  static const TypedURLSyncBridge::MergeResult DIFF_NONE =
      TypedURLSyncBridge::DIFF_NONE;
  static const TypedURLSyncBridge::MergeResult DIFF_UPDATE_NODE =
      TypedURLSyncBridge::DIFF_UPDATE_NODE;
  static const TypedURLSyncBridge::MergeResult DIFF_LOCAL_ROW_CHANGED =
      TypedURLSyncBridge::DIFF_LOCAL_ROW_CHANGED;
  static const TypedURLSyncBridge::MergeResult DIFF_LOCAL_VISITS_ADDED =
      TypedURLSyncBridge::DIFF_LOCAL_VISITS_ADDED;

  TypedURLSyncBridge* bridge() { return typed_url_sync_bridge_; }

  TypedURLSyncMetadataDatabase* metadata_store() {
    return bridge()->sync_metadata_database_;
  }

  RecordingModelTypeChangeProcessor& processor() { return *processor_; }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir test_dir_;
  scoped_refptr<TestHistoryBackend> fake_history_backend_;
  TypedURLSyncBridge* typed_url_sync_bridge_;
  // A non-owning pointer to the processor given to the bridge. Will be null
  // before being given to the bridge, to make ownership easier.
  RecordingModelTypeChangeProcessor* processor_ = nullptr;
};

// Add two typed urls locally and verify bridge can get them from GetAllData.
TEST_F(TypedURLSyncBridgeTest, GetAllData) {
  // Add two urls to backend.
  VisitVector visits1, visits2, visits3;
  URLRow row1 = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits1);
  URLRow row2 = MakeTypedUrlRow(kURL2, kTitle2, 2, 4, false, &visits2);
  URLRow expired_row = MakeTypedUrlRow("http://expired.com/", kTitle, 1,
                                       kExpiredVisit, false, &visits3);
  fake_history_backend_->SetVisitsForUrl(&row1, visits1);
  fake_history_backend_->SetVisitsForUrl(&row2, visits2);
  fake_history_backend_->SetVisitsForUrl(&expired_row, visits3);

  // Create the same data in sync.
  TypedUrlSpecifics typed_url1, typed_url2;
  WriteToTypedUrlSpecifics(row1, visits1, &typed_url1);
  WriteToTypedUrlSpecifics(row2, visits2, &typed_url2);

  // Check that the local cache is still correct, expired row is filtered out.
  VerifyAllLocalHistoryData({typed_url1, typed_url2});
}

// Add two typed urls locally and verify bridge can get them from GetData.
TEST_F(TypedURLSyncBridgeTest, GetData) {
  // Add two urls to backend.
  VisitVector visits1, visits2;
  URLRow row1 = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits1);
  URLRow row2 = MakeTypedUrlRow(kURL2, kTitle2, 2, 4, false, &visits2);
  fake_history_backend_->SetVisitsForUrl(&row1, visits1);
  fake_history_backend_->SetVisitsForUrl(&row2, visits2);

  // Create the same data in sync.
  TypedUrlSpecifics typed_url1, typed_url2;
  WriteToTypedUrlSpecifics(row1, visits1, &typed_url1);
  WriteToTypedUrlSpecifics(row2, visits2, &typed_url2);

  // Check that the local cache is still correct.
  VerifyGetData({IntToStroageKey(1)}, {typed_url1});
  VerifyGetData({IntToStroageKey(2)}, {typed_url2});
}

// Add a typed url locally and one to sync with the same data. Starting sync
// should result in no changes.
TEST_F(TypedURLSyncBridgeTest, MergeUrlNoChange) {
  // Add a url to backend.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  fake_history_backend_->SetVisitsForUrl(&row, visits);

  // Create the same data in sync.
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row, visits, typed_url);

  StartSyncing({*typed_url});
  EXPECT_TRUE(processor().put_multimap().empty());

  // Even Sync already know the url, bridge still need to tell sync about
  // storage keys.
  EXPECT_EQ(1u, processor().update_multimap().size());

  // Verify processor receive correct upate storage key.
  const auto& it = processor().update_multimap().begin();
  EXPECT_EQ(it->first, IntToStroageKey(1));
  EXPECT_TRUE(it->second->specifics.has_typed_url());
  EXPECT_EQ(it->second->specifics.typed_url().url(), kURL);
  EXPECT_EQ(it->second->specifics.typed_url().title(), kTitle);

  // Check that the local cache was is still correct.
  VerifyAllLocalHistoryData({*typed_url});
}

// Add a corupted typed url locally, has typed url count 1, but no real typed
// url visit. Starting sync should not pick up this url.
TEST_F(TypedURLSyncBridgeTest, MergeUrlNoTypedUrl) {
  // Add a url to backend.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 0, 3, false, &visits);

  // Mark typed_count to 1 even when there is no typed url visit.
  row.set_typed_count(1);
  fake_history_backend_->SetVisitsForUrl(&row, visits);

  StartSyncing(std::vector<TypedUrlSpecifics>());
  EXPECT_TRUE(processor().put_multimap().empty());

  MetadataBatch metadata_batch;
  metadata_store()->GetAllSyncMetadata(&metadata_batch);
  EXPECT_EQ(0u, metadata_batch.TakeAllMetadata().size());
}

// Starting sync with no sync data should just push the local url to sync.
TEST_F(TypedURLSyncBridgeTest, MergeUrlEmptySync) {
  // Add a url to backend.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  fake_history_backend_->SetVisitsForUrl(&row, visits);

  StartSyncing(std::vector<TypedUrlSpecifics>());

  // Check that the local cache was is still correct.
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row, visits, typed_url);
  VerifyAllLocalHistoryData({*typed_url});

  // Check that the server was updated correctly.
  ASSERT_EQ(1U, processor().put_multimap().size());
  auto recorded_specifics_iterator =
      processor().put_multimap().find(GetStorageKey(typed_url->url()));
  EXPECT_NE(processor().put_multimap().end(), recorded_specifics_iterator);
  TypedUrlSpecifics recorded_specifics =
      recorded_specifics_iterator->second->specifics.typed_url();

  ASSERT_EQ(1, recorded_specifics.visits_size());
  EXPECT_EQ(3, recorded_specifics.visits(0));
  ASSERT_EQ(1, recorded_specifics.visit_transitions_size());
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            recorded_specifics.visit_transitions(0));
}

// Starting sync with no local data should just push the synced url into the
// backend.
TEST_F(TypedURLSyncBridgeTest, MergeUrlEmptyLocal) {
  // Create the sync data.
  VisitVector visits;
  URLRow row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row, visits, typed_url);

  StartSyncing({*typed_url});
  EXPECT_EQ(0u, processor().put_multimap().size());
  EXPECT_EQ(1u, processor().update_multimap().size());

  // Verify processor receive correct upate storage key.
  const auto& it = processor().update_multimap().begin();
  EXPECT_EQ(it->first, IntToStroageKey(1));
  EXPECT_TRUE(it->second->specifics.has_typed_url());
  EXPECT_EQ(it->second->specifics.typed_url().url(), kURL);
  EXPECT_EQ(it->second->specifics.typed_url().title(), kTitle);

  // Check that the backend was updated correctly.
  VisitVector all_visits;
  Time server_time = SinceEpoch(3);
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  ASSERT_EQ(1U, all_visits.size());
  EXPECT_EQ(server_time, all_visits[0].visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[0].transition, visits[0].transition));
}

// Add a url to the local and sync data before sync begins, with the sync data
// having more recent visits. Check that starting sync updates the backend
// with the sync visit, while the older local visit is not pushed to sync.
// The title should be updated to the sync version due to the more recent
// timestamp.
TEST_F(TypedURLSyncBridgeTest, MergeUrlOldLocal) {
  // Add a url to backend.
  VisitVector visits;
  URLRow local_row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  fake_history_backend_->SetVisitsForUrl(&local_row, visits);

  // Create sync data for the same url with a more recent visit.
  VisitVector server_visits;
  URLRow server_row =
      MakeTypedUrlRow(kURL, kTitle2, 1, 6, false, &server_visits);
  server_row.set_id(fake_history_backend_->GetIdByUrl(GURL(kURL)));
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(server_row, server_visits, typed_url);
  StartSyncing({*typed_url});

  // Check that the backend was updated correctly.
  VisitVector all_visits;
  Time server_time = SinceEpoch(6);
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  ASSERT_EQ(2U, all_visits.size());
  EXPECT_EQ(server_time, all_visits.back().visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits.back().transition, server_visits[0].transition));
  URLRow url_row;
  EXPECT_TRUE(fake_history_backend_->GetURL(GURL(kURL), &url_row));
  EXPECT_EQ(kTitle2, base::UTF16ToUTF8(url_row.title()));

  // Check that the sync was updated correctly.
  // The local history visit should not be added to sync because it is older
  // than sync's oldest visit.
  ASSERT_EQ(1U, processor().put_multimap().size());

  sync_pb::TypedUrlSpecifics url_specifics = GetLastUpdateForURL(kURL);
  ASSERT_EQ(1, url_specifics.visits_size());
  EXPECT_EQ(6, url_specifics.visits(0));
  ASSERT_EQ(1, url_specifics.visit_transitions_size());
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(0));
}

// Add a url to the local and sync data before sync begins, with the local data
// having more recent visits. Check that starting sync updates the sync
// with the local visits, while the older sync visit is not pushed to the
// backend. Sync's title should be updated to the local version due to the more
// recent timestamp.
TEST_F(TypedURLSyncBridgeTest, MergeUrlOldSync) {
  // Add a url to backend.
  VisitVector visits;
  URLRow local_row = MakeTypedUrlRow(kURL, kTitle2, 1, 3, false, &visits);
  fake_history_backend_->SetVisitsForUrl(&local_row, visits);

  // Create sync data for the same url with an older visit.
  VisitVector server_visits;
  URLRow server_row =
      MakeTypedUrlRow(kURL, kTitle, 1, 2, false, &server_visits);
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(server_row, server_visits, typed_url);
  StartSyncing({*typed_url});

  // Check that the backend was not updated.
  VisitVector all_visits;
  Time local_visit_time = SinceEpoch(3);
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  ASSERT_EQ(1U, all_visits.size());
  EXPECT_EQ(local_visit_time, all_visits[0].visit_time);

  // Check that the server was updated correctly.
  // The local history visit should not be added to sync because it is older
  // than sync's oldest visit.
  ASSERT_EQ(1U, processor().put_multimap().size());

  sync_pb::TypedUrlSpecifics url_specifics = GetLastUpdateForURL(kURL);
  ASSERT_EQ(1, url_specifics.visits_size());
  EXPECT_EQ(3, url_specifics.visits(0));
  EXPECT_EQ(kTitle2, url_specifics.title());
  ASSERT_EQ(1, url_specifics.visit_transitions_size());
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(0));
}

// Check that there is no crash during start sync, if history backend and sync
// have same url, but sync has username/password in it.
// Also check sync will not accept url with username and password.
TEST_F(TypedURLSyncBridgeTest, MergeUrlsWithUsernameAndPassword) {
  const char kURLWithUsernameAndPassword[] =
      "http://username:password@pie.com/";

  // Add a url to backend.
  VisitVector visits;
  URLRow local_row = MakeTypedUrlRow(kURL, kTitle2, 1, 3, false, &visits);
  fake_history_backend_->SetVisitsForUrl(&local_row, visits);

  // Create sync data for the same url but contain username and password.
  VisitVector server_visits;
  URLRow server_row = MakeTypedUrlRow(kURLWithUsernameAndPassword, kTitle, 1, 3,
                                      false, &server_visits);
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(server_row, server_visits, typed_url);

  // Make sure there is no crash when merge two urls.
  StartSyncing({*typed_url});

  // Notify typed url sync service of the update.
  bridge()->OnURLVisited(fake_history_backend_.get(), ui::PAGE_TRANSITION_TYPED,
                         server_row, RedirectList(), SinceEpoch(7));

  // Check username/password url is not synced.
  ASSERT_EQ(1U, processor().put_multimap().size());
}

// Starting sync with both local and sync have same typed URL, but different
// visit. After merge, both local and sync should have two same visits.
TEST_F(TypedURLSyncBridgeTest, SimpleMerge) {
  // Add a url to backend.
  VisitVector visits1;
  URLRow row1 = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits1);
  fake_history_backend_->SetVisitsForUrl(&row1, visits1);

  // Create the sync data.
  VisitVector visits2;
  URLRow row2 = MakeTypedUrlRow(kURL, kTitle, 1, 4, false, &visits2);
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();
  WriteToTypedUrlSpecifics(row2, visits2, typed_url);

  StartSyncing({*typed_url});
  EXPECT_EQ(1u, processor().put_multimap().size());

  // Check that the backend was updated correctly.
  VisitVector all_visits;
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  ASSERT_EQ(2U, all_visits.size());
  EXPECT_EQ(SinceEpoch(3), all_visits[0].visit_time);
  EXPECT_EQ(SinceEpoch(4), all_visits[1].visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[0].transition, visits1[0].transition));
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[1].transition, visits2[0].transition));
}

// Create a local typed URL with one TYPED visit after sync has started. Check
// that sync is sent an ADD change for the new URL.
TEST_F(TypedURLSyncBridgeTest, AddLocalTypedUrl) {
  // Create a local typed URL (simulate a typed visit) that is not already
  // in sync. Check that sync is sent an ADD change for the existing URL.
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(std::vector<TypedUrlSpecifics>());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));

  URLRow url_row = url_rows.front();
  VisitVector visits = visit_vectors.front();

  // Check change processor.
  ASSERT_EQ(1U, processor().put_multimap().size());

  // Get typed url specifics.
  auto it = processor().put_multimap().begin();
  sync_pb::TypedUrlSpecifics url_specifics = it->second->specifics.typed_url();

  EXPECT_TRUE(URLsEqual(url_row, url_specifics));
  ASSERT_EQ(1, url_specifics.visits_size());
  ASSERT_EQ(static_cast<const int>(visits.size()), url_specifics.visits_size());
  EXPECT_EQ(visits[0].visit_time.ToInternalValue(), url_specifics.visits(0));
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(0));
}

// Update a local typed URL that is already synced. Check that sync is sent an
// UPDATE for the existing url, but RELOAD visits aren't synced.
TEST_F(TypedURLSyncBridgeTest, UpdateLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(std::vector<TypedUrlSpecifics>());

  // Update the URL row, adding another typed visit to the visit vector.
  URLRows changed_urls;
  VisitVector visits;
  URLRow url_row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_TYPED, 7, &url_row, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_RELOAD, 8, &url_row, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_LINK, 9, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(&url_row, visits);
  changed_urls.push_back(url_row);

  // Notify typed url sync service of the update.
  const auto& changes_multimap = processor().put_multimap();
  ASSERT_EQ(0U, changes_multimap.size());
  bridge()->OnURLsModified(fake_history_backend_.get(), changed_urls,
                           /*is_from_expiration=*/false);
  ASSERT_EQ(1U, changes_multimap.size());

  sync_pb::TypedUrlSpecifics url_specifics = GetLastUpdateForURL(kURL);
  EXPECT_TRUE(URLsEqual(url_row, url_specifics));
  ASSERT_EQ(3, url_specifics.visits_size());

  // Check that each visit has been translated/communicated correctly.
  // Note that the specifics record visits in chronological order, and the
  // visits from the db are in reverse chronological order.
  EXPECT_EQ(visits[0].visit_time.ToInternalValue(), url_specifics.visits(2));
  EXPECT_EQ(static_cast<const int>(visits[0].transition),
            url_specifics.visit_transitions(2));
  EXPECT_EQ(visits[2].visit_time.ToInternalValue(), url_specifics.visits(1));
  EXPECT_EQ(static_cast<const int>(visits[2].transition),
            url_specifics.visit_transitions(1));
  EXPECT_EQ(visits[3].visit_time.ToInternalValue(), url_specifics.visits(0));
  EXPECT_EQ(static_cast<const int>(visits[3].transition),
            url_specifics.visit_transitions(0));
}

// Append a RELOAD visit to a typed url that is already synced. Check that sync
// does not receive any updates.
TEST_F(TypedURLSyncBridgeTest, ReloadVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(std::vector<TypedUrlSpecifics>());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));
  const auto& changes_multimap = processor().put_multimap();
  ASSERT_EQ(1U, changes_multimap.size());

  // Update the URL row, adding another typed visit to the visit vector.
  URLRow url_row = url_rows.front();
  URLRows changed_urls;
  VisitVector new_visits;
  AddNewestVisit(ui::PAGE_TRANSITION_RELOAD, 7, &url_row, &new_visits);
  fake_history_backend_->SetVisitsForUrl(&url_row, new_visits);
  changed_urls.push_back(url_row);

  // Notify typed url sync service of the update.
  bridge()->OnURLVisited(fake_history_backend_.get(),
                         ui::PAGE_TRANSITION_RELOAD, url_row, RedirectList(),
                         SinceEpoch(7));
  // No change pass to processor
  ASSERT_EQ(1U, changes_multimap.size());
}

// Appends a LINK visit to an existing typed url. Check that sync does not
// receive any changes.
TEST_F(TypedURLSyncBridgeTest, LinkVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(std::vector<TypedUrlSpecifics>());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));
  const auto& changes_multimap = processor().put_multimap();
  ASSERT_EQ(1U, changes_multimap.size());

  // Update the URL row, adding a non-typed visit to the visit vector.
  URLRow url_row = url_rows.front();
  VisitVector new_visits;
  AddNewestVisit(ui::PAGE_TRANSITION_LINK, 6, &url_row, &new_visits);
  fake_history_backend_->SetVisitsForUrl(&url_row, new_visits);

  ui::PageTransition transition = ui::PAGE_TRANSITION_LINK;
  // Notify typed url sync service of non-typed visit, expect no change.
  bridge()->OnURLVisited(fake_history_backend_.get(), transition, url_row,
                         RedirectList(), SinceEpoch(6));
  // No change pass to processor
  ASSERT_EQ(1U, changes_multimap.size());
}

// Appends a series of LINK visits followed by a TYPED one to an existing typed
// url. Check that sync receives an UPDATE with the newest visit data.
TEST_F(TypedURLSyncBridgeTest, TypedVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(std::vector<TypedUrlSpecifics>());

  // Update the URL row, adding another typed visit to the visit vector.
  VisitVector visits;
  URLRow url_row = MakeTypedUrlRow(kURL, kTitle, 1, 3, false, &visits);
  AddOldestVisit(ui::PAGE_TRANSITION_LINK, 1, &url_row, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_LINK, 6, &url_row, &visits);
  AddNewestVisit(ui::PAGE_TRANSITION_TYPED, 7, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(&url_row, visits);

  // Notify typed url sync service of typed visit.
  const auto& changes_multimap = processor().put_multimap();
  ASSERT_EQ(0U, changes_multimap.size());
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  bridge()->OnURLVisited(fake_history_backend_.get(), transition, url_row,
                         RedirectList(), Time());

  ASSERT_EQ(1U, changes_multimap.size());
  sync_pb::TypedUrlSpecifics url_specifics = GetLastUpdateForURL(kURL);

  EXPECT_TRUE(URLsEqual(url_row, url_specifics));
  EXPECT_EQ(4, url_specifics.visits_size());

  // Check that each visit has been translated/communicated correctly.
  // Note that the specifics record visits in chronological order, and the
  // visits from the db are in reverse chronological order.
  int r = url_specifics.visits_size() - 1;
  for (int i = 0; i < url_specifics.visits_size(); ++i, --r) {
    EXPECT_EQ(visits[i].visit_time.ToInternalValue(), url_specifics.visits(r));
    EXPECT_EQ(static_cast<const int>(visits[i].transition),
              url_specifics.visit_transitions(r));
  }
}

// Delete several (but not all) local typed urls. Check that sync receives the
// DELETE changes, and the non-deleted urls remain synced.
TEST_F(TypedURLSyncBridgeTest, DeleteLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back("http://pie.com/");
  urls.push_back("http://cake.com/");
  urls.push_back("http://google.com/");
  urls.push_back("http://foo.com/");

  StartSyncing(std::vector<TypedUrlSpecifics>());
  ASSERT_TRUE(BuildAndPushLocalChanges(4, 0, urls, &url_rows, &visit_vectors));
  const auto& changes_multimap = processor().put_multimap();
  ASSERT_EQ(4U, changes_multimap.size());

  // Delete some urls from backend and create deleted row vector.
  URLRows rows;
  std::set<std::string> deleted_storage_keys;
  for (size_t i = 0; i < 3u; ++i) {
    std::string storage_key = GetStorageKey(url_rows[i].url().spec());
    deleted_storage_keys.insert(storage_key);
    fake_history_backend_->DeleteURL(url_rows[i].url());
    rows.push_back(url_rows[i]);
  }

  // Notify typed url sync service.
  bridge()->OnURLsDeleted(fake_history_backend_.get(), false, false, rows,
                          std::set<GURL>());

  const auto& delete_set = processor().delete_set();
  ASSERT_EQ(3U, delete_set.size());
  for (const std::string& storage_key : delete_set) {
    EXPECT_TRUE(deleted_storage_keys.find(storage_key) !=
                deleted_storage_keys.end());
    deleted_storage_keys.erase(storage_key);
  }
  ASSERT_TRUE(deleted_storage_keys.empty());
}

// Delete the last typed visit for one (but not all) local typed urls. Check
// that sync receives the DELETE changes, and the non-deleted urls remain
// synced.
TEST_F(TypedURLSyncBridgeTest, DeleteLocalTypedUrlVisit) {
  VisitVector visits1, visits2;
  URLRow row1 = MakeTypedUrlRowWithTwoVisits(kURL, kTitle,
                                             /*typed_visit_time=*/2,
                                             /*reload_visit_time=*/4,
                                             /*hidden=*/false, &visits1);
  URLRow row2 = MakeTypedUrlRow(kURL2, kTitle2, /*typed_count=*/2,
                                /*last_visit=*/10, false, &visits2);
  fake_history_backend_->SetVisitsForUrl(&row1, visits1);
  fake_history_backend_->SetVisitsForUrl(&row2, visits2);

  StartSyncing({});

  // Simulate deletion of the last typed visit (e.g. by clearing browsing data),
  // the deletion must get synced up.
  fake_history_backend_->ExpireHistoryBetween({},
                                              /*begin_time=*/SinceEpoch(1),
                                              /*end_time=*/SinceEpoch(3),
                                              /*user_initiated*/ true);
  URLRow row1_updated;
  ASSERT_TRUE(fake_history_backend_->GetURL(GURL(kURL), &row1_updated));
  URLRows changed_urls{row1_updated};
  bridge()->OnURLsModified(fake_history_backend_.get(), changed_urls,
                           /*is_from_expiration=*/false);

  const auto& delete_set = processor().delete_set();
  EXPECT_EQ(1U, delete_set.size());
  EXPECT_EQ(1U, delete_set.count(GetStorageKey(kURL)));
}

// Expire several (but not all) local typed urls. This has only impact on local
// store (metadata in the db and in-memory maps), nothing gets synced up.
TEST_F(TypedURLSyncBridgeTest, ExpireLocalTypedUrl) {
  StartSyncing(std::vector<TypedUrlSpecifics>());

  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back("http://pie.com/");
  urls.push_back("http://cake.com/");
  urls.push_back("http://google.com/");
  urls.push_back("http://foo.com/");
  urls.push_back("http://bar.com/");

  // Add the URLs into the history db and notify the bridge.
  ASSERT_TRUE(BuildAndPushLocalChanges(5, 0, urls, &url_rows, &visit_vectors));
  ASSERT_EQ(5U, processor().put_multimap().size());
  int previous_put_size = processor().put_multimap().size();
  // Store the typed_urls incl. metadata into the bridge's database.
  for (const std::string& url : urls) {
    ApplyUrlAndVisitsChange(url, kTitle, /*typed_count=*/1, /*last_visit=*/3,
                            /*hidden=*/false, /*update_metadata=*/true,
                            EntityChange::ACTION_ADD);
  }

  // Check all the metadata is here, no need to untrack anything so far.
  MetadataBatch metadata_batch;
  metadata_store()->GetAllSyncMetadata(&metadata_batch);
  ASSERT_EQ(5u, metadata_batch.TakeAllMetadata().size());
  ASSERT_EQ(0U, processor().untrack_for_storage_key_set().size());

  // Simulate expiration - delete some urls from the backend and create deleted
  // row vector.
  URLRows rows;
  std::set<std::string> deleted_storage_keys;
  for (size_t i = 0; i < 3u; ++i) {
    std::string storage_key = GetStorageKey(url_rows[i].url().spec());
    deleted_storage_keys.insert(storage_key);
    fake_history_backend_->DeleteURL(url_rows[i].url());
    rows.push_back(url_rows[i]);
  }

  // Notify typed url sync service of these URLs getting expired.
  bridge()->OnURLsDeleted(fake_history_backend_.get(), /*all_history=*/false,
                          /*expired=*/true, rows, std::set<GURL>());

  // This does not propagate to the processor.
  EXPECT_EQ(0U, processor().put_multimap().size() - previous_put_size);
  EXPECT_EQ(0U, processor().delete_set().size());
  // The processor is still informed to clear its in-memory maps.
  EXPECT_EQ(3U, processor().untrack_for_storage_key_set().size());

  // The urls are removed from the metadata store.
  MetadataBatch smaller_metadata_batch;
  metadata_store()->GetAllSyncMetadata(&smaller_metadata_batch);
  EXPECT_EQ(2u, smaller_metadata_batch.GetAllMetadata().size());
  for (const auto& kv : smaller_metadata_batch.GetAllMetadata()) {
    EXPECT_TRUE(deleted_storage_keys.find(kv.first) ==
                deleted_storage_keys.end());
  }
}

// Saturate the visits for a typed url with both TYPED and LINK navigations.
// Check that no more than kMaxTypedURLVisits are synced, and that LINK visits
// are dropped rather than TYPED ones.
TEST_F(TypedURLSyncBridgeTest, MaxVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(std::vector<TypedUrlSpecifics>());
  ASSERT_TRUE(BuildAndPushLocalChanges(0, 1, urls, &url_rows, &visit_vectors));
  const auto& changes_multimap = processor().put_multimap();
  ASSERT_EQ(0U, changes_multimap.size());

  URLRow url_row = url_rows.front();
  VisitVector visits;

  // Add |kMaxTypedUrlVisits| + 10 visits to the url. The 10 oldest
  // non-typed visits are expected to be skipped.
  int i = 1;
  for (; i <= kMaxTypedUrlVisits - 20; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_TYPED, i, &url_row, &visits);
  for (; i <= kMaxTypedUrlVisits; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_LINK, i, &url_row, &visits);
  for (; i <= kMaxTypedUrlVisits + 10; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_TYPED, i, &url_row, &visits);

  fake_history_backend_->SetVisitsForUrl(&url_row, visits);

  // Notify typed url sync service of typed visit.
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  bridge()->OnURLVisited(fake_history_backend_.get(), transition, url_row,
                         RedirectList(), Time());

  ASSERT_EQ(1U, changes_multimap.size());
  sync_pb::TypedUrlSpecifics url_specifics = GetLastUpdateForURL(kURL);
  ASSERT_EQ(kMaxTypedUrlVisits, url_specifics.visits_size());

  // Check that each visit has been translated/communicated correctly.
  // Note that the specifics records visits in chronological order, and the
  // visits from the db are in reverse chronological order.
  int num_typed_visits_synced = 0;
  int num_other_visits_synced = 0;
  int r = url_specifics.visits_size() - 1;
  for (int i = 0; i < url_specifics.visits_size(); ++i, --r) {
    if (url_specifics.visit_transitions(i) ==
        static_cast<int32_t>(ui::PAGE_TRANSITION_TYPED)) {
      ++num_typed_visits_synced;
    } else {
      ++num_other_visits_synced;
    }
  }
  EXPECT_EQ(kMaxTypedUrlVisits - 10, num_typed_visits_synced);
  EXPECT_EQ(10, num_other_visits_synced);
}

// Add enough visits to trigger throttling of updates to a typed url. Check that
// sync does not receive an update until the proper throttle interval has been
// reached.
TEST_F(TypedURLSyncBridgeTest, ThrottleVisitLocalTypedUrl) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(std::vector<TypedUrlSpecifics>());
  ASSERT_TRUE(BuildAndPushLocalChanges(0, 1, urls, &url_rows, &visit_vectors));
  const auto& changes_multimap = processor().put_multimap();
  ASSERT_EQ(0U, changes_multimap.size());

  URLRow url_row = url_rows.front();
  VisitVector visits;

  // Add enough visits to the url so that typed count is above the throttle
  // limit, and not right on the interval that gets synced.
  int i = 1;
  for (; i < kVisitThrottleThreshold + kVisitThrottleMultiple / 2; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_TYPED, i, &url_row, &visits);
  fake_history_backend_->SetVisitsForUrl(&url_row, visits);

  // Notify typed url sync service of typed visit.
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  bridge()->OnURLVisited(fake_history_backend_.get(), transition, url_row,
                         RedirectList(), Time());

  // Should throttle, so sync and local cache should not update.
  ASSERT_EQ(0U, changes_multimap.size());

  visits.clear();
  for (; i % kVisitThrottleMultiple != 1; ++i)
    AddNewestVisit(ui::PAGE_TRANSITION_TYPED, i, &url_row, &visits);
  --i;  // Account for the increment before the condition ends.
  fake_history_backend_->SetVisitsForUrl(&url_row, visits);

  // Notify typed url sync service of typed visit.
  bridge()->OnURLVisited(fake_history_backend_.get(), transition, url_row,
                         RedirectList(), Time());

  ASSERT_EQ(1U, changes_multimap.size());
  sync_pb::TypedUrlSpecifics url_specifics = GetLastUpdateForURL(kURL);
  ASSERT_EQ(i, url_specifics.visits_size());
}

// Create a remote typed URL and visit, then send to sync bridge after sync
// has started. Check that local DB is received the new URL and visit.
TEST_F(TypedURLSyncBridgeTest, AddUrlAndVisits) {
  StartSyncing(std::vector<TypedUrlSpecifics>());
  VisitVector visits = ApplyUrlAndVisitsChange(
      kURL, kTitle, /*typed_count=*/1, /*last_visit=*/3,
      /*hidden=*/false,
      /*update_metadata=*/false, EntityChange::ACTION_ADD);

  ASSERT_EQ(0U, processor().put_multimap().size());
  ASSERT_EQ(1U, processor().update_multimap().size());
  ASSERT_EQ(0U, processor().untrack_for_client_tag_hash_set().size());

  // Verify processor receive correct upate storage key.
  const auto& it = processor().update_multimap().begin();
  EXPECT_EQ(it->first, IntToStroageKey(1));
  EXPECT_TRUE(it->second->specifics.has_typed_url());
  EXPECT_EQ(it->second->specifics.typed_url().url(), kURL);
  EXPECT_EQ(it->second->specifics.typed_url().title(), kTitle);

  Time visit_time = SinceEpoch(3);
  VisitVector all_visits;
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  EXPECT_EQ(1U, all_visits.size());
  EXPECT_EQ(visit_time, all_visits[0].visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[0].transition, visits[0].transition));
  URLRow url_row;
  EXPECT_TRUE(fake_history_backend_->GetURL(GURL(kURL), &url_row));
  EXPECT_EQ(kTitle, base::UTF16ToUTF8(url_row.title()));
}

// Create a remote typed URL with expired visit, then send to sync bridge after
// sync has started. Check that local DB did not receive the expired URL and
// visit.
TEST_F(TypedURLSyncBridgeTest, AddExpiredUrlAndVisits) {
  StartSyncing(std::vector<TypedUrlSpecifics>());
  VisitVector visits = ApplyUrlAndVisitsChange(
      kURL, kTitle, /*typed_count=*/1, /*last_visit=*/kExpiredVisit,
      /*hidden=*/false, /*update_metadata=*/false, EntityChange::ACTION_ADD);

  ASSERT_EQ(0U, processor().put_multimap().size());
  ASSERT_EQ(0U, processor().update_multimap().size());
  ASSERT_EQ(1U, processor().untrack_for_client_tag_hash_set().size());

  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_EQ(0, url_id);
}

// Update a remote typed URL and create a new visit that is already synced, then
// send the update to sync bridge. Check that local DB is received an
// UPDATE for the existing url and new visit.
TEST_F(TypedURLSyncBridgeTest, UpdateUrlAndVisits) {
  StartSyncing(std::vector<TypedUrlSpecifics>());

  VisitVector visits = ApplyUrlAndVisitsChange(
      kURL, kTitle, /*typed_count=*/1, /*last_visit=*/3,
      /*hidden=*/false,
      /*update_metadata=*/false, EntityChange::ACTION_ADD);
  Time visit_time = SinceEpoch(3);
  VisitVector all_visits;
  URLRow url_row;

  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);

  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);

  EXPECT_EQ(1U, all_visits.size());
  EXPECT_EQ(visit_time, all_visits[0].visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[0].transition, visits[0].transition));
  EXPECT_TRUE(fake_history_backend_->GetURL(GURL(kURL), &url_row));
  EXPECT_EQ(kTitle, base::UTF16ToUTF8(url_row.title()));

  VisitVector new_visits = ApplyUrlAndVisitsChange(
      kURL, kTitle2, /*typed_count=*/2, /*last_visit=*/6,
      /*hidden=*/false,
      /*update_metadata=*/false, EntityChange::ACTION_UPDATE);

  Time new_visit_time = SinceEpoch(6);
  url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);

  EXPECT_EQ(2U, all_visits.size());
  EXPECT_EQ(new_visit_time, all_visits.back().visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits.back().transition, new_visits[0].transition));
  EXPECT_TRUE(fake_history_backend_->GetURL(GURL(kURL), &url_row));
  EXPECT_EQ(kTitle2, base::UTF16ToUTF8(url_row.title()));
}

// Delete a typed urls which already synced. Check that local DB receives the
// DELETE changes.
TEST_F(TypedURLSyncBridgeTest, DeleteUrlAndVisits) {
  URLRows url_rows;
  std::vector<VisitVector> visit_vectors;
  std::vector<std::string> urls;
  urls.push_back(kURL);

  StartSyncing(std::vector<TypedUrlSpecifics>());
  ASSERT_TRUE(BuildAndPushLocalChanges(1, 0, urls, &url_rows, &visit_vectors));
  const auto& changes_multimap = processor().put_multimap();
  ASSERT_EQ(1U, changes_multimap.size());

  Time visit_time = SinceEpoch(3);
  VisitVector all_visits;
  URLID url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_NE(0, url_id);
  fake_history_backend_->GetVisitsForURL(url_id, &all_visits);
  EXPECT_EQ(1U, all_visits.size());
  EXPECT_EQ(visit_time, all_visits[0].visit_time);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      all_visits[0].transition, visit_vectors[0][0].transition));
  URLRow url_row;
  EXPECT_TRUE(fake_history_backend_->GetURL(GURL(kURL), &url_row));
  EXPECT_EQ(kTitle, base::UTF16ToUTF8(url_row.title()));

  // Add observer back to check if TypedUrlSyncBridge receive delete
  // changes back from fake_history_backend_.
  AddObserver();

  ApplyUrlAndVisitsChange(kURL, kTitle, /*typed_count=*/1, /*last_visit=*/3,
                          /*hidden=*/false, /*update_metadata=*/false,
                          EntityChange::ACTION_DELETE);

  EXPECT_FALSE(fake_history_backend_->GetURL(GURL(kURL), &url_row));
  url_id = fake_history_backend_->GetIdByUrl(GURL(kURL));
  ASSERT_EQ(0, url_id);

  // Check TypedUrlSyncBridge did not receive update since the update is
  // trigered by it.
  ASSERT_EQ(1U, changes_multimap.size());
}

// Create two set of visits for history DB and sync DB, two same set of visits
// are same. Check DiffVisits will return empty set of diff visits.
TEST_F(TypedURLSyncBridgeTest, DiffVisitsSame) {
  VisitVector old_visits;
  sync_pb::TypedUrlSpecifics new_url;

  const int64_t visits[] = {1024, 2065, 65534, 1237684};

  for (int64_t visit : visits) {
    old_visits.push_back(
        VisitRow(0, SinceEpoch(visit), 0, ui::PAGE_TRANSITION_TYPED, 0, true));
    new_url.add_visits(visit);
    new_url.add_visit_transitions(ui::PAGE_TRANSITION_TYPED);
  }

  std::vector<VisitInfo> new_visits;
  VisitVector removed_visits;

  DiffVisits(old_visits, new_url, &new_visits, &removed_visits);
  EXPECT_TRUE(new_visits.empty());
  EXPECT_TRUE(removed_visits.empty());
}

// Create two set of visits for history DB and sync DB. Check DiffVisits will
// return correct set of diff visits.
TEST_F(TypedURLSyncBridgeTest, DiffVisitsRemove) {
  VisitVector old_visits;
  sync_pb::TypedUrlSpecifics new_url;

  const int64_t visits_left[] = {1,    2,     1024,    1500,   2065,
                                 6000, 65534, 1237684, 2237684};
  const int64_t visits_right[] = {1024, 2065, 65534, 1237684};

  // DiffVisits will not remove the first visit, because we never delete visits
  // from the start of the array (since those visits can get truncated by the
  // size-limiting code).
  const int64_t visits_removed[] = {1500, 6000, 2237684};

  for (int64_t visit : visits_left) {
    old_visits.push_back(
        VisitRow(0, SinceEpoch(visit), 0, ui::PAGE_TRANSITION_TYPED, 0, true));
  }

  for (int64_t visit : visits_right) {
    new_url.add_visits(visit);
    new_url.add_visit_transitions(ui::PAGE_TRANSITION_TYPED);
  }

  std::vector<VisitInfo> new_visits;
  VisitVector removed_visits;

  DiffVisits(old_visits, new_url, &new_visits, &removed_visits);
  EXPECT_TRUE(new_visits.empty());
  ASSERT_EQ(removed_visits.size(), base::size(visits_removed));
  for (size_t i = 0; i < base::size(visits_removed); ++i) {
    EXPECT_EQ(removed_visits[i].visit_time.ToInternalValue(),
              visits_removed[i]);
  }
}

// Create two set of visits for history DB and sync DB. Check DiffVisits will
// return correct set of diff visits.
TEST_F(TypedURLSyncBridgeTest, DiffVisitsAdd) {
  VisitVector old_visits;
  sync_pb::TypedUrlSpecifics new_url;

  const int64_t visits_left[] = {1024, 2065, 65534, 1237684};
  const int64_t visits_right[] = {1,    1024,  1500,    2065,
                                  6000, 65534, 1237684, 2237684};

  const int64_t visits_added[] = {1, 1500, 6000, 2237684};

  for (int64_t visit : visits_left) {
    old_visits.push_back(
        VisitRow(0, SinceEpoch(visit), 0, ui::PAGE_TRANSITION_TYPED, 0, true));
  }

  for (int64_t visit : visits_right) {
    new_url.add_visits(visit);
    new_url.add_visit_transitions(ui::PAGE_TRANSITION_TYPED);
  }

  std::vector<VisitInfo> new_visits;
  VisitVector removed_visits;

  DiffVisits(old_visits, new_url, &new_visits, &removed_visits);
  EXPECT_TRUE(removed_visits.empty());
  ASSERT_TRUE(new_visits.size() == base::size(visits_added));
  for (size_t i = 0; i < base::size(visits_added); ++i) {
    EXPECT_EQ(new_visits[i].first.ToInternalValue(), visits_added[i]);
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        new_visits[i].second, ui::PAGE_TRANSITION_TYPED));
  }
}

// Create three visits, check RELOAD visit is removed by
// WriteToTypedUrlSpecifics so it won't apply to sync DB.
TEST_F(TypedURLSyncBridgeTest, WriteTypedUrlSpecifics) {
  VisitVector visits;
  visits.push_back(CreateVisit(ui::PAGE_TRANSITION_TYPED, 1));
  visits.push_back(CreateVisit(ui::PAGE_TRANSITION_RELOAD, 2));
  visits.push_back(CreateVisit(ui::PAGE_TRANSITION_LINK, 3));

  URLRow url(MakeTypedUrlRow(kURL, kTitle, 0, 100, false, &visits));
  sync_pb::TypedUrlSpecifics typed_url;
  WriteToTypedUrlSpecifics(url, visits, &typed_url);
  // RELOAD visits should be removed.
  EXPECT_EQ(2, typed_url.visits_size());
  EXPECT_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  EXPECT_EQ(1, typed_url.visits(0));
  EXPECT_EQ(3, typed_url.visits(1));
  EXPECT_EQ(static_cast<int32_t>(ui::PAGE_TRANSITION_TYPED),
            typed_url.visit_transitions(0));
  EXPECT_EQ(static_cast<int32_t>(ui::PAGE_TRANSITION_LINK),
            typed_url.visit_transitions(1));
}

// Create 101 visits, check WriteToTypedUrlSpecifics will only keep 100 visits.
TEST_F(TypedURLSyncBridgeTest, TooManyVisits) {
  VisitVector visits;
  int64_t timestamp = 1000;
  visits.push_back(CreateVisit(ui::PAGE_TRANSITION_TYPED, timestamp++));
  for (int i = 0; i < 100; ++i) {
    visits.push_back(CreateVisit(ui::PAGE_TRANSITION_LINK, timestamp++));
  }
  URLRow url(MakeTypedUrlRow(kURL, kTitle, 0, timestamp++, false, &visits));
  sync_pb::TypedUrlSpecifics typed_url;
  WriteToTypedUrlSpecifics(url, visits, &typed_url);
  // # visits should be capped at 100.
  EXPECT_EQ(100, typed_url.visits_size());
  EXPECT_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  EXPECT_EQ(1000, typed_url.visits(0));
  // Visit with timestamp of 1001 should be omitted since we should have
  // skipped that visit to stay under the cap.
  EXPECT_EQ(1002, typed_url.visits(1));
  EXPECT_EQ(static_cast<int32_t>(ui::PAGE_TRANSITION_TYPED),
            typed_url.visit_transitions(0));
  EXPECT_EQ(static_cast<int32_t>(ui::PAGE_TRANSITION_LINK),
            typed_url.visit_transitions(1));
}

// Create 306 visits, check WriteToTypedUrlSpecifics will only keep 100 typed
// visits.
TEST_F(TypedURLSyncBridgeTest, TooManyTypedVisits) {
  VisitVector visits;
  int64_t timestamp = 1000;
  for (int i = 0; i < 102; ++i) {
    visits.push_back(CreateVisit(ui::PAGE_TRANSITION_TYPED, timestamp++));
    visits.push_back(CreateVisit(ui::PAGE_TRANSITION_LINK, timestamp++));
    visits.push_back(CreateVisit(ui::PAGE_TRANSITION_RELOAD, timestamp++));
  }
  URLRow url(MakeTypedUrlRow(kURL, kTitle, 0, timestamp++, false, &visits));
  sync_pb::TypedUrlSpecifics typed_url;
  WriteToTypedUrlSpecifics(url, visits, &typed_url);
  // # visits should be capped at 100.
  EXPECT_EQ(100, typed_url.visits_size());
  EXPECT_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  // First two typed visits should be skipped.
  EXPECT_EQ(1006, typed_url.visits(0));

  // Ensure there are no non-typed visits since that's all that should fit.
  for (int i = 0; i < typed_url.visits_size(); ++i) {
    EXPECT_EQ(static_cast<int32_t>(ui::PAGE_TRANSITION_TYPED),
              typed_url.visit_transitions(i));
  }
}

// Create a typed url without visit, check WriteToTypedUrlSpecifics will return
// false for it.
TEST_F(TypedURLSyncBridgeTest, NoTypedVisits) {
  VisitVector visits;
  URLRow url(MakeTypedUrlRow(kURL, kTitle, 0, 1000, false, &visits));
  sync_pb::TypedUrlSpecifics typed_url;
  EXPECT_FALSE(WriteToTypedUrlSpecifics(url, visits, &typed_url));
  // URLs with no typed URL visits should not been written to specifics.
  EXPECT_EQ(0, typed_url.visits_size());
}

TEST_F(TypedURLSyncBridgeTest, MergeUrls) {
  VisitVector visits1;
  URLRow row1(MakeTypedUrlRow(kURL, kTitle, 2, 3, false, &visits1));
  sync_pb::TypedUrlSpecifics specs1(
      MakeTypedUrlSpecifics(kURL, kTitle, 3, false));
  URLRow new_row1((GURL(kURL)));
  std::vector<VisitInfo> new_visits1;
  EXPECT_TRUE(TypedURLSyncBridgeTest::MergeUrls(specs1, row1, &visits1,
                                                &new_row1, &new_visits1) ==
              TypedURLSyncBridgeTest::DIFF_NONE);

  VisitVector visits2;
  URLRow row2(MakeTypedUrlRow(kURL, kTitle, 2, 3, false, &visits2));
  sync_pb::TypedUrlSpecifics specs2(
      MakeTypedUrlSpecifics(kURL, kTitle, 3, true));
  VisitVector expected_visits2;
  URLRow expected2(
      MakeTypedUrlRow(kURL, kTitle, 2, 3, true, &expected_visits2));
  URLRow new_row2((GURL(kURL)));
  std::vector<VisitInfo> new_visits2;
  EXPECT_TRUE(TypedURLSyncBridgeTest::MergeUrls(specs2, row2, &visits2,
                                                &new_row2, &new_visits2) ==
              TypedURLSyncBridgeTest::DIFF_LOCAL_ROW_CHANGED);
  EXPECT_TRUE(URLsEqual(new_row2, expected2));

  VisitVector visits3;
  URLRow row3(MakeTypedUrlRow(kURL, kTitle, 2, 3, false, &visits3));
  sync_pb::TypedUrlSpecifics specs3(
      MakeTypedUrlSpecifics(kURL, kTitle2, 3, true));
  VisitVector expected_visits3;
  URLRow expected3(
      MakeTypedUrlRow(kURL, kTitle2, 2, 3, true, &expected_visits3));
  URLRow new_row3((GURL(kURL)));
  std::vector<VisitInfo> new_visits3;
  EXPECT_EQ(TypedURLSyncBridgeTest::DIFF_LOCAL_ROW_CHANGED |
                TypedURLSyncBridgeTest::DIFF_NONE,
            TypedURLSyncBridgeTest::MergeUrls(specs3, row3, &visits3, &new_row3,
                                              &new_visits3));
  EXPECT_TRUE(URLsEqual(new_row3, expected3));

  // Create one node in history DB with timestamp of 3, and one node in sync
  // DB with timestamp of 4. Result should contain one new item (4).
  VisitVector visits4;
  URLRow row4(MakeTypedUrlRow(kURL, kTitle, 2, 3, false, &visits4));
  sync_pb::TypedUrlSpecifics specs4(
      MakeTypedUrlSpecifics(kURL, kTitle2, 4, false));
  VisitVector expected_visits4;
  URLRow expected4(
      MakeTypedUrlRow(kURL, kTitle2, 2, 4, false, &expected_visits4));
  URLRow new_row4((GURL(kURL)));
  std::vector<VisitInfo> new_visits4;
  EXPECT_EQ(TypedURLSyncBridgeTest::DIFF_UPDATE_NODE |
                TypedURLSyncBridgeTest::DIFF_LOCAL_ROW_CHANGED |
                TypedURLSyncBridgeTest::DIFF_LOCAL_VISITS_ADDED,
            TypedURLSyncBridgeTest::MergeUrls(specs4, row4, &visits4, &new_row4,
                                              &new_visits4));
  EXPECT_EQ(1U, new_visits4.size());
  EXPECT_EQ(specs4.visits(0), new_visits4[0].first.ToInternalValue());
  EXPECT_TRUE(URLsEqual(new_row4, expected4));
  EXPECT_EQ(2U, visits4.size());

  VisitVector visits5;
  URLRow row5(MakeTypedUrlRow(kURL, kTitle, 1, 4, false, &visits5));
  sync_pb::TypedUrlSpecifics specs5(
      MakeTypedUrlSpecifics(kURL, kTitle, 3, false));
  VisitVector expected_visits5;
  URLRow expected5(
      MakeTypedUrlRow(kURL, kTitle, 2, 3, false, &expected_visits5));
  URLRow new_row5((GURL(kURL)));
  std::vector<VisitInfo> new_visits5;

  // UPDATE_NODE should be set because row5 has a newer last_visit timestamp.
  EXPECT_EQ(TypedURLSyncBridgeTest::DIFF_UPDATE_NODE |
                TypedURLSyncBridgeTest::DIFF_NONE,
            TypedURLSyncBridgeTest::MergeUrls(specs5, row5, &visits5, &new_row5,
                                              &new_visits5));
  EXPECT_TRUE(URLsEqual(new_row5, expected5));
  EXPECT_EQ(0U, new_visits5.size());
}

// Tests to ensure that we don't resurrect expired URLs (URLs that have been
// deleted from the history DB but still exist in the sync DB).
TEST_F(TypedURLSyncBridgeTest, MergeUrlsAfterExpiration) {
  // First, create a history row that has two visits, with timestamps 2 and 3.
  VisitVector(history_visits);
  history_visits.push_back(
      VisitRow(0, SinceEpoch(2), 0, ui::PAGE_TRANSITION_TYPED, 0, true));
  URLRow history_url(
      MakeTypedUrlRow(kURL, kTitle, 2, 3, false, &history_visits));

  // Now, create a sync node with visits at timestamps 1, 2, 3, 4.
  sync_pb::TypedUrlSpecifics node(
      MakeTypedUrlSpecifics(kURL, kTitle, 1, false));
  node.add_visits(2);
  node.add_visits(3);
  node.add_visits(4);
  node.add_visit_transitions(2);
  node.add_visit_transitions(3);
  node.add_visit_transitions(4);
  URLRow new_history_url(history_url.url());
  std::vector<VisitInfo> new_visits;
  EXPECT_EQ(
      TypedURLSyncBridgeTest::DIFF_NONE |
          TypedURLSyncBridgeTest::DIFF_LOCAL_VISITS_ADDED,
      TypedURLSyncBridgeTest::MergeUrls(node, history_url, &history_visits,
                                        &new_history_url, &new_visits));
  EXPECT_TRUE(URLsEqual(history_url, new_history_url));
  EXPECT_EQ(1U, new_visits.size());
  EXPECT_EQ(4U, new_visits[0].first.ToInternalValue());
  // We should not sync the visit with timestamp #1 since it is earlier than
  // any other visit for this URL in the history DB. But we should sync visit
  // #4.
  EXPECT_EQ(3U, history_visits.size());
  EXPECT_EQ(2U, history_visits[0].visit_time.ToInternalValue());
  EXPECT_EQ(3U, history_visits[1].visit_time.ToInternalValue());
  EXPECT_EQ(4U, history_visits[2].visit_time.ToInternalValue());
}

// Create a local typed URL with one expired TYPED visit,
// MergeSyncData should not pass it to sync. And then add a non
// expired visit, OnURLsModified should only send the non expired visit to sync.
TEST_F(TypedURLSyncBridgeTest, LocalExpiredTypedUrlDoNotSync) {
  URLRow row;
  URLRows changed_urls;
  VisitVector visits;

  // Add an expired typed URL to local.
  row = MakeTypedUrlRow(kURL, kTitle, 1, kExpiredVisit, false, &visits);
  fake_history_backend_->SetVisitsForUrl(&row, visits);

  StartSyncing(std::vector<TypedUrlSpecifics>());

  // Check change processor did not receive expired typed URL.
  const auto& changes_multimap = processor().put_multimap();
  ASSERT_EQ(0U, changes_multimap.size());

  // Add a non expired typed URL to local.
  row = MakeTypedUrlRow(kURL, kTitle, 2, 1, false, &visits);
  fake_history_backend_->SetVisitsForUrl(&row, visits);

  changed_urls.push_back(row);
  // Notify typed url sync service of the update.
  bridge()->OnURLsModified(fake_history_backend_.get(), changed_urls,
                           /*is_from_expiration=*/false);

  // Check change processor did not receive expired typed URL.
  ASSERT_EQ(1U, changes_multimap.size());

  // Get typed url specifics. Verify only a non-expired visit received.
  sync_pb::TypedUrlSpecifics url_specifics = GetLastUpdateForURL(kURL);

  EXPECT_TRUE(URLsEqual(row, url_specifics));
  ASSERT_EQ(1, url_specifics.visits_size());
  ASSERT_EQ(static_cast<const int>(visits.size() - 1),
            url_specifics.visits_size());
  EXPECT_EQ(visits[1].visit_time.ToInternalValue(), url_specifics.visits(0));
  EXPECT_EQ(static_cast<const int>(visits[1].transition),
            url_specifics.visit_transitions(0));
}

// Tests that database error gets reported to processor as model type error.
TEST_F(TypedURLSyncBridgeTest, DatabaseError) {
  bridge()->OnDatabaseError();
  EXPECT_TRUE(processor().GetError().has_value());
}

}  // namespace history
