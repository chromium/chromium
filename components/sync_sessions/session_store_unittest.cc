// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_store.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/test/test_matchers.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/sync_sessions/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {
namespace {

using sync_pb::SessionSpecifics;
using syncer::DataBatch;
using syncer::EntityData;
using syncer::EntityMetadataMap;
using syncer::HasEncryptionKeyName;
using syncer::IsEmptyMetadataBatch;
using syncer::MetadataBatch;
using syncer::MetadataBatchContains;
using syncer::ModelTypeStore;
using syncer::NoModelError;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::Matcher;
using testing::NiceMock;
using testing::NotNull;
using testing::Pair;
using testing::Return;
using testing::UnorderedElementsAre;
using testing::_;

const char kCacheGuid[] = "SomeCacheGuid";

// A mock callback that a) can be used as mock to verify call expectations and
// b) conveniently exposes the last instantiated session store.
class MockOpenCallback {
 public:
  MOCK_METHOD3(Run,
               void(const base::Optional<syncer::ModelError>& error,
                    SessionStore* store,
                    MetadataBatch* metadata_batch));

  SessionStore::OpenCallback Get() {
    return base::BindOnce(
        [](MockOpenCallback* callback,
           const base::Optional<syncer::ModelError>& error,
           std::unique_ptr<SessionStore> store,
           std::unique_ptr<MetadataBatch> metadata_batch) {
          // Store a copy of the pointer for GetResult().
          callback->store_ = std::move(store);
          // Call mock method.
          callback->Run(error, callback->store_.get(), metadata_batch.get());
          callback->loop_.Quit();
        },
        base::Unretained(this));
  }

  // Waits until the callback gets triggered.
  void Wait() { loop_.Run(); }

  SessionStore* GetResult() { return store_.get(); }

  std::unique_ptr<SessionStore> StealResult() { return std::move(store_); }

 private:
  base::RunLoop loop_;
  std::unique_ptr<SessionStore> store_;
};

MATCHER_P(EntityDataHasSpecifics, session_specifics_matcher, "") {
  return session_specifics_matcher.MatchAndExplain(arg.specifics.session(),
                                                   result_listener);
}

std::map<std::string, EntityData> BatchToEntityDataMap(
    std::unique_ptr<DataBatch> batch) {
  std::map<std::string, EntityData> storage_key_to_data;
  while (batch && batch->HasNext()) {
    auto batch_entry = batch->Next();
    const std::string& storage_key = batch_entry.first;
    std::unique_ptr<EntityData> entity_data = std::move(batch_entry.second);
    EXPECT_THAT(entity_data, NotNull());
    if (entity_data) {
      storage_key_to_data.emplace(storage_key, std::move(*entity_data));
    }
  }
  return storage_key_to_data;
}

std::unique_ptr<MetadataBatch> ReadAllPersistedMetadataFrom(
    ModelTypeStore* store) {
  std::unique_ptr<MetadataBatch> batch;
  base::RunLoop loop;
  store->ReadAllMetadata(base::BindOnce(
      [](std::unique_ptr<MetadataBatch>* output_batch, base::RunLoop* loop,
         const base::Optional<syncer::ModelError>& error,
         std::unique_ptr<MetadataBatch> input_batch) {
        EXPECT_FALSE(error) << error->ToString();
        EXPECT_THAT(input_batch, NotNull());
        *output_batch = std::move(input_batch);
        loop->Quit();
      },
      &batch, &loop));
  loop.Run();
  return batch;
}

std::map<std::string, SessionSpecifics> ReadAllPersistedDataFrom(
    ModelTypeStore* store) {
  std::unique_ptr<ModelTypeStore::RecordList> records;
  base::RunLoop loop;
  store->ReadAllData(base::BindOnce(
      [](std::unique_ptr<ModelTypeStore::RecordList>* output_records,
         base::RunLoop* loop, const base::Optional<syncer::ModelError>& error,
         std::unique_ptr<ModelTypeStore::RecordList> input_records) {
        EXPECT_FALSE(error) << error->ToString();
        EXPECT_THAT(input_records, NotNull());
        *output_records = std::move(input_records);
        loop->Quit();
      },
      &records, &loop));
  loop.Run();
  std::map<std::string, SessionSpecifics> result;
  if (records) {
    for (const ModelTypeStore::Record& record : *records) {
      SessionSpecifics specifics;
      EXPECT_TRUE(specifics.ParseFromString(record.value));
      result.emplace(record.id, specifics);
    }
  }
  return result;
}

class SessionStoreOpenTest : public ::testing::Test {
 protected:
  SessionStoreOpenTest()
      : session_sync_prefs_(&pref_service_),
        underlying_store_(
            syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(
                syncer::SESSIONS)) {
    SessionSyncPrefs::RegisterProfilePrefs(pref_service_.registry());

    mock_sync_sessions_client_ =
        std::make_unique<testing::NiceMock<MockSyncSessionsClient>>();

    ON_CALL(*mock_sync_sessions_client_, GetSessionSyncPrefs())
        .WillByDefault(Return(&session_sync_prefs_));
    ON_CALL(*mock_sync_sessions_client_, GetStoreFactory())
        .WillByDefault(
            Return(syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
                underlying_store_.get())));
  }

  ~SessionStoreOpenTest() override {}

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  SessionSyncPrefs session_sync_prefs_;
  std::unique_ptr<MockSyncSessionsClient> mock_sync_sessions_client_;
  testing::NiceMock<
      base::MockCallback<SessionStore::RestoredForeignTabCallback>>
      mock_restored_foreign_tab_callback_;
  std::unique_ptr<ModelTypeStore> underlying_store_;
};

TEST_F(SessionStoreOpenTest, ShouldCreateStore) {
  ASSERT_THAT(session_sync_prefs_.GetSyncSessionsGUID(), IsEmpty());

  MockOpenCallback completion;
  EXPECT_CALL(completion, Run(NoModelError(), /*store=*/NotNull(),
                              MetadataBatchContains(_, IsEmpty())));
  SessionStore::Open(kCacheGuid, mock_restored_foreign_tab_callback_.Get(),
                     mock_sync_sessions_client_.get(), completion.Get());
  completion.Wait();
  ASSERT_THAT(completion.GetResult(), NotNull());
  EXPECT_THAT(completion.GetResult()->local_session_info().client_name,
              Eq(syncer::GetPersonalizableDeviceNameBlocking()));
  EXPECT_THAT(session_sync_prefs_.GetSyncSessionsGUID(),
              Eq(std::string("session_sync") + kCacheGuid));
}

TEST_F(SessionStoreOpenTest, ShouldReadSessionsGuidFromPrefs) {
  const std::string kCachedGuid = "cachedguid1";
  session_sync_prefs_.SetSyncSessionsGUID(kCachedGuid);

  NiceMock<MockOpenCallback> completion;
  SessionStore::Open(kCacheGuid, mock_restored_foreign_tab_callback_.Get(),
                     mock_sync_sessions_client_.get(), completion.Get());
  completion.Wait();
  ASSERT_THAT(completion.GetResult(), NotNull());
  EXPECT_THAT(completion.GetResult()->local_session_info().session_tag,
              Eq(kCachedGuid));
  EXPECT_THAT(session_sync_prefs_.GetSyncSessionsGUID(), Eq(kCachedGuid));
}

TEST_F(SessionStoreOpenTest, ShouldNotUseClientIfCancelled) {
  // Mimics a caller that uses a weak pointer.
  class Caller {
   public:
    explicit Caller(SessionStore::OpenCallback cb) : cb_(std::move(cb)) {}

    SessionStore::OpenCallback GetCancelableCallback() {
      return base::BindOnce(&Caller::Completed, weak_ptr_factory_.GetWeakPtr());
    }

   private:
    void Completed(const base::Optional<syncer::ModelError>& error,
                   std::unique_ptr<SessionStore> store,
                   std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
      std::move(cb_).Run(error, std::move(store), std::move(metadata_batch));
    }

    SessionStore::OpenCallback cb_;
    base::WeakPtrFactory<Caller> weak_ptr_factory_{this};
  };

  NiceMock<MockOpenCallback> mock_completion;
  auto caller = std::make_unique<Caller>(mock_completion.Get());

  EXPECT_CALL(mock_completion, Run(_, _, _)).Times(0);

  SessionStore::Open(kCacheGuid, mock_restored_foreign_tab_callback_.Get(),
                     mock_sync_sessions_client_.get(),
                     caller->GetCancelableCallback());

  // The client gets destroyed before callback completion.
  mock_sync_sessions_client_.reset();
  caller.reset();

  // Run until idle to test for crashes due to use-after-free.
  base::RunLoop().RunUntilIdle();
}

// Test fixture that creates an initial session store.
class SessionStoreTest : public SessionStoreOpenTest {
 protected:
  const std::string kLocalSessionTag = "localsessiontag";

  SessionStoreTest() {
    session_sync_prefs_.SetSyncSessionsGUID(kLocalSessionTag);
    session_store_ = CreateSessionStore();
  }

  std::unique_ptr<SessionStore> CreateSessionStore() {
    NiceMock<MockOpenCallback> completion;
    SessionStore::Open(kCacheGuid, mock_restored_foreign_tab_callback_.Get(),
                       mock_sync_sessions_client_.get(), completion.Get());
    completion.Wait();
    EXPECT_THAT(completion.GetResult(), NotNull());
    return completion.StealResult();
  }

  SessionStore* session_store() { return session_store_.get(); }

 private:
  std::unique_ptr<SessionStore> session_store_;
};

TEST_F(SessionStoreTest, ShouldCreateLocalSession) {
  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);

  EXPECT_THAT(BatchToEntityDataMap(session_store()->GetAllSessionData()),
              ElementsAre(Pair(header_storage_key,
                               EntityDataHasSpecifics(MatchesHeader(
                                   kLocalSessionTag, /*window_ids=*/{},
                                   /*tab_ids=*/{})))));
  // Verify that GetSessionDataForKeys() returns the header entity.
  EXPECT_THAT(BatchToEntityDataMap(
                  session_store()->GetSessionDataForKeys({header_storage_key})),
              ElementsAre(Pair(header_storage_key,
                               EntityDataHasSpecifics(MatchesHeader(
                                   kLocalSessionTag, /*window_ids=*/{},
                                   /*tab_ids=*/{})))));

  // Verify the underlying storage does NOT contain the data.
  EXPECT_THAT(ReadAllPersistedDataFrom(underlying_store_.get()), IsEmpty());

  // Verify the underlying storage does NOT contain metadata.
  EXPECT_THAT(ReadAllPersistedMetadataFrom(underlying_store_.get()),
              IsEmptyMetadataBatch());
}

TEST_F(SessionStoreTest, ShouldWriteAndRestoreMetadata) {
  const std::string kStorageKey1 = "TestStorageKey1";
  const std::string kServerId1 = "TestServerId1";
  const std::string kEncryptionKeyName1 = "TestEncryptionKey1";

  // Populate with metadata.
  std::unique_ptr<SessionStore::WriteBatch> batch =
      session_store()->CreateWriteBatch(/*error_handler=*/base::DoNothing());
  ASSERT_THAT(batch, NotNull());

  sync_pb::EntityMetadata metadata1;
  metadata1.set_server_id(kServerId1);
  batch->GetMetadataChangeList()->UpdateMetadata(kStorageKey1, metadata1);

  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_encryption_key_name(kEncryptionKeyName1);
  batch->GetMetadataChangeList()->UpdateModelTypeState(model_type_state);

  SessionStore::WriteBatch::Commit(std::move(batch));

  // Verify the underlying storage contains the metadata.
  EXPECT_THAT(ReadAllPersistedMetadataFrom(underlying_store_.get()),
              MetadataBatchContains(HasEncryptionKeyName(kEncryptionKeyName1),
                                    ElementsAre(Pair(kStorageKey1, _))));

  // Create second session store.
  NiceMock<MockOpenCallback> completion;
  EXPECT_CALL(completion, Run(NoModelError(), /*store=*/NotNull(),
                              MetadataBatchContains(
                                  HasEncryptionKeyName(kEncryptionKeyName1),
                                  ElementsAre(Pair(kStorageKey1, _)))));
  SessionStore::Open(kCacheGuid, mock_restored_foreign_tab_callback_.Get(),
                     mock_sync_sessions_client_.get(), completion.Get());
  completion.Wait();
  EXPECT_THAT(completion.GetResult(), NotNull());
  EXPECT_NE(session_store(), completion.GetResult());
}

TEST_F(SessionStoreTest, ShouldUpdateTrackerWithForeignData) {
  const std::string kForeignSessionTag = "SomeForeignTag";
  const int kWindowId = 5;
  const int kTabId1 = 7;
  const int kTabId2 = 8;
  const int kTabNodeId1 = 2;
  const int kTabNodeId2 = 3;

  EXPECT_CALL(mock_restored_foreign_tab_callback_, Run(_, _)).Times(0);

  ASSERT_THAT(session_store()->tracker()->LookupAllForeignSessions(
                  SyncedSessionTracker::RAW),
              IsEmpty());

  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kForeignSessionTag);
  const std::string tab_storage_key1 =
      SessionStore::GetTabStorageKey(kForeignSessionTag, kTabNodeId1);
  const std::string tab_storage_key2 =
      SessionStore::GetTabStorageKey(kForeignSessionTag, kTabNodeId2);
  ASSERT_THAT(BatchToEntityDataMap(session_store()->GetSessionDataForKeys(
                  {header_storage_key, tab_storage_key1, tab_storage_key2})),
              IsEmpty());

  // Populate with data.
  SessionSpecifics header;
  header.set_session_tag(kForeignSessionTag);
  header.mutable_header()->add_window()->set_window_id(kWindowId);
  header.mutable_header()->mutable_window(0)->add_tab(kTabId1);
  header.mutable_header()->mutable_window(0)->add_tab(kTabId2);
  ASSERT_TRUE(SessionStore::AreValidSpecifics(header));

  SessionSpecifics tab1;
  tab1.set_session_tag(kForeignSessionTag);
  tab1.set_tab_node_id(kTabNodeId1);
  tab1.mutable_tab()->set_window_id(kWindowId);
  tab1.mutable_tab()->set_tab_id(kTabId1);
  ASSERT_TRUE(SessionStore::AreValidSpecifics(tab1));

  SessionSpecifics tab2;
  tab2.set_session_tag(kForeignSessionTag);
  tab2.set_tab_node_id(kTabNodeId2);
  tab2.mutable_tab()->set_window_id(kWindowId);
  tab2.mutable_tab()->set_tab_id(kTabId2);
  ASSERT_TRUE(SessionStore::AreValidSpecifics(tab2));

  std::unique_ptr<SessionStore::WriteBatch> batch =
      session_store()->CreateWriteBatch(/*error_handler=*/base::DoNothing());
  ASSERT_THAT(batch, NotNull());
  batch->PutAndUpdateTracker(header, base::Time::Now());
  batch->PutAndUpdateTracker(tab1, base::Time::Now());
  batch->PutAndUpdateTracker(tab2, base::Time::Now());
  SessionStore::WriteBatch::Commit(std::move(batch));

  EXPECT_THAT(session_store()->tracker()->LookupAllForeignSessions(
                  SyncedSessionTracker::RAW),
              ElementsAre(MatchesSyncedSession(
                  kForeignSessionTag, {{kWindowId, {kTabId1, kTabId2}}})));
  EXPECT_THAT(
      BatchToEntityDataMap(session_store()->GetSessionDataForKeys(
          {header_storage_key, tab_storage_key1, tab_storage_key2})),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(MatchesHeader(
                   kForeignSessionTag, {kWindowId}, {kTabId1, kTabId2}))),
          Pair(tab_storage_key1,
               EntityDataHasSpecifics(MatchesTab(kForeignSessionTag, kWindowId,
                                                 kTabId1, kTabNodeId1,
                                                 /*urls=*/_))),
          Pair(tab_storage_key2,
               EntityDataHasSpecifics(MatchesTab(kForeignSessionTag, kWindowId,
                                                 kTabId2, kTabNodeId2,
                                                 /*urls=*/_)))));
}

TEST_F(SessionStoreTest, ShouldWriteAndRestoreForeignData) {
  const std::string kForeignSessionTag = "SomeForeignTag";
  const int kWindowId = 5;
  const int kTabId1 = 7;
  const int kTabNodeId1 = 2;

  EXPECT_CALL(mock_restored_foreign_tab_callback_, Run(_, _)).Times(0);

  const std::string local_header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);

  ASSERT_THAT(session_store()->tracker()->LookupAllForeignSessions(
                  SyncedSessionTracker::RAW),
              IsEmpty());
  // Local session is automatically created.
  ASSERT_THAT(BatchToEntityDataMap(session_store()->GetAllSessionData()),
              ElementsAre(Pair(local_header_storage_key, _)));
  ASSERT_THAT(ReadAllPersistedDataFrom(underlying_store_.get()), IsEmpty());

  // Populate with data.
  SessionSpecifics header;
  header.set_session_tag(kForeignSessionTag);
  header.mutable_header()->add_window()->set_window_id(kWindowId);
  header.mutable_header()->mutable_window(0)->add_tab(kTabId1);
  ASSERT_TRUE(SessionStore::AreValidSpecifics(header));

  SessionSpecifics tab1;
  tab1.set_session_tag(kForeignSessionTag);
  tab1.set_tab_node_id(kTabNodeId1);
  tab1.mutable_tab()->set_window_id(kWindowId);
  tab1.mutable_tab()->set_tab_id(kTabId1);
  ASSERT_TRUE(SessionStore::AreValidSpecifics(tab1));

  std::unique_ptr<SessionStore::WriteBatch> batch =
      session_store()->CreateWriteBatch(/*error_handler=*/base::DoNothing());
  ASSERT_THAT(batch, NotNull());
  batch->PutAndUpdateTracker(header, base::Time::Now());
  batch->PutAndUpdateTracker(tab1, base::Time::Now());

  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kForeignSessionTag);
  const std::string tab_storage_key1 =
      SessionStore::GetTabStorageKey(kForeignSessionTag, kTabNodeId1);

  sync_pb::EntityMetadata header_metadata;
  header_metadata.set_server_id("someserverid1");
  batch->GetMetadataChangeList()->UpdateMetadata(header_storage_key,
                                                 header_metadata);

  sync_pb::EntityMetadata tab1_metadata;
  tab1_metadata.set_server_id("someserverid2");
  batch->GetMetadataChangeList()->UpdateMetadata(tab_storage_key1,
                                                 tab1_metadata);

  SessionStore::WriteBatch::Commit(std::move(batch));

  // Verify the underlying storage contains the data.
  ASSERT_THAT(
      ReadAllPersistedDataFrom(underlying_store_.get()),
      UnorderedElementsAre(
          Pair(header_storage_key,
               MatchesHeader(kForeignSessionTag, {kWindowId}, {kTabId1})),
          Pair(tab_storage_key1,
               MatchesTab(kForeignSessionTag, kWindowId, kTabId1, kTabNodeId1,
                          /*urls=*/_))));

  // Verify tracker exposes the foreign tabs.
  ASSERT_THAT(
      session_store()->tracker()->LookupAllForeignSessions(
          SyncedSessionTracker::RAW),
      ElementsAre(MatchesSyncedSession(
          kForeignSessionTag, {{kWindowId, std::vector<int>{kTabId1}}})));

  // Creation of a second session store should trigger a callback for the
  // restored tab.
  EXPECT_CALL(mock_restored_foreign_tab_callback_,
              Run(testing::Property(&sync_pb::SessionTab::tab_id, kTabId1), _));

  // Create second session store to verify that the persisted state is restored,
  // by mimicing a Chrome restart and using |underlying_store_| (in-memory) as a
  // replacement for on-disk persistence.
  std::unique_ptr<SessionStore> restored_store = CreateSessionStore();
  ASSERT_THAT(restored_store, NotNull());
  ASSERT_NE(session_store(), restored_store.get());

  // Verify tracker was restored.
  EXPECT_THAT(
      restored_store->tracker()->LookupAllForeignSessions(
          SyncedSessionTracker::RAW),
      ElementsAre(MatchesSyncedSession(
          kForeignSessionTag, {{kWindowId, std::vector<int>{kTabId1}}})));

  EXPECT_THAT(BatchToEntityDataMap(restored_store->GetAllSessionData()),
              UnorderedElementsAre(
                  Pair(local_header_storage_key, _),
                  Pair(header_storage_key,
                       EntityDataHasSpecifics(MatchesHeader(
                           kForeignSessionTag, {kWindowId}, {kTabId1}))),
                  Pair(tab_storage_key1,
                       EntityDataHasSpecifics(MatchesTab(
                           kForeignSessionTag, kWindowId, kTabId1, kTabNodeId1,
                           /*urls=*/_)))));

  EXPECT_THAT(BatchToEntityDataMap(session_store()->GetSessionDataForKeys(
                  {header_storage_key, tab_storage_key1})),
              UnorderedElementsAre(
                  Pair(header_storage_key,
                       EntityDataHasSpecifics(MatchesHeader(
                           kForeignSessionTag, {kWindowId}, {kTabId1}))),
                  Pair(tab_storage_key1,
                       EntityDataHasSpecifics(MatchesTab(
                           kForeignSessionTag, kWindowId, kTabId1, kTabNodeId1,
                           /*urls=*/_)))));
}

TEST_F(SessionStoreTest, ShouldDeleteForeignData) {
  const std::string kForeignSessionTag = "SomeForeignTag";
  const int kWindowId = 5;
  const int kTabId1 = 7;
  const int kTabId2 = 8;
  const int kTabNodeId1 = 1;
  const int kTabNodeId2 = 2;

  EXPECT_CALL(mock_restored_foreign_tab_callback_, Run(_, _)).Times(0);

  const std::string local_header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);

  // Local session is automatically created.
  ASSERT_THAT(BatchToEntityDataMap(session_store()->GetAllSessionData()),
              ElementsAre(Pair(local_header_storage_key, _)));
  ASSERT_THAT(ReadAllPersistedDataFrom(underlying_store_.get()), IsEmpty());

  // Populate with foreign data: one header entity and two tabs.
  SessionSpecifics header;
  header.set_session_tag(kForeignSessionTag);
  header.mutable_header()->add_window()->set_window_id(kWindowId);
  header.mutable_header()->mutable_window(0)->add_tab(kTabId1);
  header.mutable_header()->mutable_window(0)->add_tab(kTabId2);
  ASSERT_TRUE(SessionStore::AreValidSpecifics(header));

  SessionSpecifics tab1;
  tab1.set_session_tag(kForeignSessionTag);
  tab1.set_tab_node_id(kTabNodeId1);
  tab1.mutable_tab()->set_window_id(kWindowId);
  tab1.mutable_tab()->set_tab_id(kTabId1);
  ASSERT_TRUE(SessionStore::AreValidSpecifics(tab1));

  SessionSpecifics tab2;
  tab2.set_session_tag(kForeignSessionTag);
  tab2.set_tab_node_id(kTabNodeId2);
  tab2.mutable_tab()->set_window_id(kWindowId);
  tab2.mutable_tab()->set_tab_id(kTabId2);
  ASSERT_TRUE(SessionStore::AreValidSpecifics(tab2));

  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kForeignSessionTag);
  const std::string tab_storage_key1 =
      SessionStore::GetTabStorageKey(kForeignSessionTag, kTabNodeId1);
  const std::string tab_storage_key2 =
      SessionStore::GetTabStorageKey(kForeignSessionTag, kTabNodeId2);

  // Write data and update the tracker.
  {
    std::unique_ptr<SessionStore::WriteBatch> batch =
        session_store()->CreateWriteBatch(/*error_handler=*/base::DoNothing());
    ASSERT_THAT(batch, NotNull());
    batch->PutAndUpdateTracker(header, base::Time::Now());
    batch->PutAndUpdateTracker(tab1, base::Time::Now());
    batch->PutAndUpdateTracker(tab2, base::Time::Now());

    sync_pb::EntityMetadata header_metadata;
    header_metadata.set_server_id("someserverid1");
    batch->GetMetadataChangeList()->UpdateMetadata(header_storage_key,
                                                   header_metadata);

    sync_pb::EntityMetadata tab1_metadata;
    tab1_metadata.set_server_id("someserverid2");
    batch->GetMetadataChangeList()->UpdateMetadata(tab_storage_key1,
                                                   tab1_metadata);

    sync_pb::EntityMetadata tab2_metadata;
    tab2_metadata.set_server_id("someserverid3");
    batch->GetMetadataChangeList()->UpdateMetadata(tab_storage_key2,
                                                   tab2_metadata);
    SessionStore::WriteBatch::Commit(std::move(batch));
  }

  // Verify the underlying storage contains the data.
  ASSERT_THAT(
      ReadAllPersistedDataFrom(underlying_store_.get()),
      UnorderedElementsAre(
          Pair(header_storage_key,
               MatchesHeader(kForeignSessionTag, {kWindowId},
                             {kTabId1, kTabId2})),
          Pair(tab_storage_key1,
               MatchesTab(kForeignSessionTag, kWindowId, kTabId1, kTabNodeId1,
                          /*urls=*/_)),
          Pair(tab_storage_key2,
               MatchesTab(kForeignSessionTag, kWindowId, kTabId2, kTabNodeId2,
                          /*urls=*/_))));

  // Verify tracker exposes the foreign tabs.
  ASSERT_THAT(session_store()->tracker()->LookupAllForeignSessions(
                  SyncedSessionTracker::RAW),
              ElementsAre(MatchesSyncedSession(
                  kForeignSessionTag,
                  {{kWindowId, std::vector<int>{kTabId1, kTabId2}}})));

  // Mimic receiving a tab deletion for |tab1|, which should only affect that
  // entity.
  {
    std::unique_ptr<SessionStore::WriteBatch> batch =
        session_store()->CreateWriteBatch(/*error_handler=*/base::DoNothing());

    EXPECT_THAT(batch->DeleteForeignEntityAndUpdateTracker(tab_storage_key1),
                ElementsAre(tab_storage_key1));

    SessionStore::WriteBatch::Commit(std::move(batch));
  }

  EXPECT_THAT(
      ReadAllPersistedDataFrom(underlying_store_.get()),
      UnorderedElementsAre(
          Pair(header_storage_key,
               MatchesHeader(kForeignSessionTag, {kWindowId},
                             {kTabId1, kTabId2})),
          Pair(tab_storage_key2,
               MatchesTab(kForeignSessionTag, kWindowId, kTabId2, kTabNodeId2,
                          /*urls=*/_))));

  // Mimic receiving a header deletion (which should delete all remaining
  // entities for that session).
  {
    std::unique_ptr<SessionStore::WriteBatch> batch =
        session_store()->CreateWriteBatch(/*error_handler=*/base::DoNothing());

    EXPECT_THAT(batch->DeleteForeignEntityAndUpdateTracker(header_storage_key),
                ElementsAre(header_storage_key, tab_storage_key2));

    SessionStore::WriteBatch::Commit(std::move(batch));
  }

  EXPECT_THAT(session_store()->tracker()->LookupAllForeignSessions(
                  SyncedSessionTracker::RAW),
              IsEmpty());
  EXPECT_THAT(ReadAllPersistedDataFrom(underlying_store_.get()), IsEmpty());
}

TEST_F(SessionStoreTest, ShouldReturnForeignUnmappedTabs) {
  const std::string kForeignSessionTag = "SomeForeignTag";
  const int kWindowId = 5;
  const int kTabId1 = 7;
  const int kTabNodeId1 = 2;

  const std::string local_header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);
  const std::string foreign_header_storage_key =
      SessionStore::GetHeaderStorageKey(kForeignSessionTag);
  const std::string foreign_tab_storage_key =
      SessionStore::GetTabStorageKey(kForeignSessionTag, kTabNodeId1);

  // Local header entity is present initially.
  ASSERT_THAT(BatchToEntityDataMap(session_store()->GetAllSessionData()),
              ElementsAre(Pair(local_header_storage_key, _)));

  SessionSpecifics tab1;
  tab1.set_session_tag(kForeignSessionTag);
  tab1.set_tab_node_id(kTabNodeId1);
  tab1.mutable_tab()->set_window_id(kWindowId);
  tab1.mutable_tab()->set_tab_id(kTabId1);
  ASSERT_TRUE(SessionStore::AreValidSpecifics(tab1));

  std::unique_ptr<SessionStore::WriteBatch> batch =
      session_store()->CreateWriteBatch(/*error_handler=*/base::DoNothing());
  ASSERT_THAT(batch, NotNull());
  batch->PutAndUpdateTracker(tab1, base::Time::Now());
  SessionStore::WriteBatch::Commit(std::move(batch));

  EXPECT_THAT(BatchToEntityDataMap(session_store()->GetAllSessionData()),
              UnorderedElementsAre(
                  Pair(local_header_storage_key, _),
                  Pair(foreign_header_storage_key,
                       EntityDataHasSpecifics(MatchesHeader(kForeignSessionTag,
                                                            /*window_ids=*/{},
                                                            /*tab_ids=*/{}))),
                  Pair(foreign_tab_storage_key,
                       EntityDataHasSpecifics(MatchesTab(
                           kForeignSessionTag, kWindowId, kTabId1, kTabNodeId1,
                           /*urls=*/_)))));
}

TEST_F(SessionStoreTest, ShouldIgnoreForeignOrphanTabs) {
  const std::string kForeignSessionTag = "SomeForeignTag";
  const int kWindowId = 5;
  const int kTabId = 7;
  // Both tab nodes point to the same tab ID, so the second one should prevail.
  const int kTabNodeId1 = 2;
  const int kTabNodeId2 = 3;

  const std::string local_header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);
  const std::string foreign_header_storage_key =
      SessionStore::GetHeaderStorageKey(kForeignSessionTag);
  const std::string foreign_tab_storage_key2 =
      SessionStore::GetTabStorageKey(kForeignSessionTag, kTabNodeId2);

  // Local header entity is present initially.
  ASSERT_THAT(BatchToEntityDataMap(session_store()->GetAllSessionData()),
              ElementsAre(Pair(local_header_storage_key, _)));

  SessionSpecifics tab1;
  tab1.set_session_tag(kForeignSessionTag);
  tab1.set_tab_node_id(kTabNodeId1);
  tab1.mutable_tab()->set_window_id(kWindowId);
  tab1.mutable_tab()->set_tab_id(kTabId);
  ASSERT_TRUE(SessionStore::AreValidSpecifics(tab1));

  SessionSpecifics tab2;
  tab2.set_session_tag(kForeignSessionTag);
  tab2.set_tab_node_id(kTabNodeId2);
  tab2.mutable_tab()->set_window_id(kWindowId);
  tab2.mutable_tab()->set_tab_id(kTabId);
  ASSERT_TRUE(SessionStore::AreValidSpecifics(tab2));

  // Store the two foreign tabs, in order.
  std::unique_ptr<SessionStore::WriteBatch> batch =
      session_store()->CreateWriteBatch(/*error_handler=*/base::DoNothing());
  ASSERT_THAT(batch, NotNull());
  batch->PutAndUpdateTracker(tab1, base::Time::Now());
  batch->PutAndUpdateTracker(tab2, base::Time::Now());
  SessionStore::WriteBatch::Commit(std::move(batch));

  // The first foreign tab should have been overwritten by the second one,
  // because they shared a tab ID.
  EXPECT_THAT(BatchToEntityDataMap(session_store()->GetAllSessionData()),
              UnorderedElementsAre(
                  Pair(local_header_storage_key, _),
                  Pair(foreign_header_storage_key,
                       EntityDataHasSpecifics(MatchesHeader(kForeignSessionTag,
                                                            /*window_ids=*/{},
                                                            /*tab_ids=*/{}))),
                  Pair(foreign_tab_storage_key2,
                       EntityDataHasSpecifics(MatchesTab(
                           kForeignSessionTag, kWindowId, kTabId, kTabNodeId2,
                           /*urls=*/_)))));
}

}  // namespace
}  // namespace sync_sessions
