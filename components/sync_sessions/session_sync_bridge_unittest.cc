// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_sync_bridge.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/model/sync_metadata_store.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/test_matchers.h"
#include "components/sync_sessions/favicon_cache.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/sync_sessions/tab_node_pool.h"
#include "components/sync_sessions/test_matchers.h"
#include "components/sync_sessions/test_synced_window_delegates_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {
namespace {

using sync_pb::EntityMetadata;
using sync_pb::SessionSpecifics;
using syncer::DataBatch;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::IsEmptyMetadataBatch;
using syncer::MetadataBatch;
using syncer::MetadataChangeList;
using syncer::MockModelTypeChangeProcessor;
using testing::_;
using testing::AtLeast;
using testing::Contains;
using testing::ElementsAre;
using testing::Eq;
using testing::InSequence;
using testing::IsEmpty;
using testing::IsNull;
using testing::Matcher;
using testing::Not;
using testing::NotNull;
using testing::Pair;
using testing::Pointee;
using testing::Return;
using testing::SaveArg;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using testing::WithArg;

const char kLocalSessionTag[] = "sessiontag1";

MATCHER_P(EntityDataHasSpecifics, session_specifics_matcher, "") {
  return session_specifics_matcher.MatchAndExplain(arg->specifics.session(),
                                                   result_listener);
}

std::unique_ptr<syncer::EntityData> SpecificsToEntity(
    const sync_pb::SessionSpecifics& specifics,
    base::Time mtime = base::Time::Now()) {
  auto data = std::make_unique<syncer::EntityData>();
  data->client_tag_hash = syncer::ClientTagHash::FromUnhashed(
      syncer::SESSIONS, SessionStore::GetClientTag(specifics));
  *data->specifics.mutable_session() = specifics;
  data->modification_time = mtime;
  return data;
}

std::unique_ptr<syncer::UpdateResponseData> SpecificsToUpdateResponse(
    const sync_pb::SessionSpecifics& specifics,
    base::Time mtime = base::Time::Now()) {
  auto data = std::make_unique<syncer::UpdateResponseData>();
  data->entity = SpecificsToEntity(specifics, mtime);
  return data;
}

std::map<std::string, std::unique_ptr<EntityData>> BatchToEntityDataMap(
    std::unique_ptr<DataBatch> batch) {
  std::map<std::string, std::unique_ptr<EntityData>> storage_key_to_data;
  while (batch && batch->HasNext()) {
    storage_key_to_data.insert(batch->Next());
  }
  return storage_key_to_data;
}

std::unique_ptr<syncer::UpdateResponseData> CreateTombstone(
    const std::string& client_tag) {
  auto tombstone = std::make_unique<syncer::EntityData>();

  tombstone->client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(syncer::SESSIONS, client_tag);

  auto data = std::make_unique<syncer::UpdateResponseData>();
  data->entity = std::move(tombstone);
  data->response_version = 2;
  return data;
}

syncer::CommitResponseData CreateSuccessResponse(
    const std::string& client_tag) {
  syncer::CommitResponseData response;
  response.client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(syncer::SESSIONS, client_tag);
  response.sequence_number = 1;
  return response;
}

sync_pb::SessionSpecifics CreateHeaderSpecificsWithOneTab(
    const std::string& session_tag,
    int window_id,
    int tab_id) {
  sync_pb::SessionSpecifics specifics;
  specifics.set_session_tag(session_tag);
  specifics.mutable_header()->set_client_name("Some client name");
  specifics.mutable_header()->set_device_type(
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
  sync_pb::SessionWindow* window = specifics.mutable_header()->add_window();
  window->set_browser_type(sync_pb::SessionWindow_BrowserType_TYPE_TABBED);
  window->set_window_id(window_id);
  window->add_tab(tab_id);
  return specifics;
}

sync_pb::SessionSpecifics CreateTabSpecifics(const std::string& session_tag,
                                             int window_id,
                                             int tab_id,
                                             int tab_node_id,
                                             const std::string& url) {
  sync_pb::SessionSpecifics specifics;
  specifics.set_session_tag(session_tag);
  specifics.set_tab_node_id(tab_node_id);
  specifics.mutable_tab()->add_navigation()->set_virtual_url(url);
  specifics.mutable_tab()->set_window_id(window_id);
  specifics.mutable_tab()->set_tab_id(tab_id);
  return specifics;
}

class SessionSyncBridgeTest : public ::testing::Test {
 protected:
  SessionSyncBridgeTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(
            syncer::SESSIONS)),
        session_sync_prefs_(&pref_service_),
        favicon_cache_(/*favicon_service=*/nullptr,
                       /*history_service=*/nullptr,
                       /*max_sync_favicon_limit=*/0) {
    SessionSyncPrefs::RegisterProfilePrefs(pref_service_.registry());

    ON_CALL(mock_sync_sessions_client_, GetSessionSyncPrefs())
        .WillByDefault(Return(&session_sync_prefs_));
    ON_CALL(mock_sync_sessions_client_, GetStoreFactory())
        .WillByDefault(
            Return(syncer::ModelTypeStoreTestUtil::FactoryForForwardingStore(
                store_.get())));
    ON_CALL(mock_sync_sessions_client_, GetSyncedWindowDelegatesGetter())
        .WillByDefault(Return(&window_getter_));
    ON_CALL(mock_sync_sessions_client_, GetLocalSessionEventRouter())
        .WillByDefault(Return(window_getter_.router()));

    session_sync_prefs_.SetSyncSessionsGUID(kLocalSessionTag);

    // Even if we use NiceMock, let's be strict about errors and let tests
    // explicitly list them.
    EXPECT_CALL(mock_processor_, ReportError(_)).Times(0);
  }

  ~SessionSyncBridgeTest() override {}

  void InitializeBridge() {
    real_processor_ =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::SESSIONS, /*dump_stack=*/base::DoNothing(),
            /*commit_only=*/false);
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
    // Instantiate the bridge.
    bridge_ = std::make_unique<SessionSyncBridge>(
        mock_foreign_session_updated_cb_.Get(), &mock_sync_sessions_client_,
        mock_processor_.CreateForwardingProcessor());
  }

  void ShutdownBridge() {
    bridge_.reset();
    // The mock is still delegating to |real_processor_|, so we reset it too.
    ASSERT_TRUE(testing::Mock::VerifyAndClear(&mock_processor_));
    real_processor_.reset();
  }

  void StartSyncing(const std::vector<SessionSpecifics>& remote_data = {}) {
    syncer::DataTypeActivationRequest request;
    request.error_handler = base::DoNothing();
    request.cache_guid = "TestCacheGuid";
    request.authenticated_account_id = CoreAccountId("SomeAccountId");

    base::RunLoop loop;
    real_processor_->OnSyncStarting(
        request,
        base::BindLambdaForTesting(
            [&loop](std::unique_ptr<syncer::DataTypeActivationResponse>) {
              loop.Quit();
            }));
    loop.Run();

    sync_pb::ModelTypeState state;
    state.set_initial_sync_done(true);
    state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(syncer::SESSIONS));
    syncer::UpdateResponseDataList initial_updates;
    for (const SessionSpecifics& specifics : remote_data) {
      initial_updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, std::move(initial_updates));
  }

  std::map<std::string, std::unique_ptr<EntityData>> GetAllData() {
    base::RunLoop loop;
    std::unique_ptr<DataBatch> batch;
    bridge_->GetAllDataForDebugging(base::BindLambdaForTesting(
        [&loop, &batch](std::unique_ptr<DataBatch> input_batch) {
          batch = std::move(input_batch);
          loop.Quit();
        }));
    loop.Run();
    EXPECT_NE(nullptr, batch);
    return BatchToEntityDataMap(std::move(batch));
  }

  std::map<std::string, std::unique_ptr<EntityData>> GetData(
      const std::vector<std::string>& storage_keys) {
    base::RunLoop loop;
    std::unique_ptr<DataBatch> batch;
    bridge_->GetData(
        storage_keys,
        base::BindLambdaForTesting(
            [&loop, &batch](std::unique_ptr<DataBatch> input_batch) {
              batch = std::move(input_batch);
              loop.Quit();
            }));
    loop.Run();
    EXPECT_NE(nullptr, batch);
    return BatchToEntityDataMap(std::move(batch));
  }

  std::unique_ptr<EntityData> GetData(const std::string& storage_key) {
    std::map<std::string, std::unique_ptr<EntityData>> entity_data_map =
        GetData(std::vector<std::string>{storage_key});
    EXPECT_LE(entity_data_map.size(), 1U);
    if (entity_data_map.empty()) {
      return nullptr;
    }
    EXPECT_EQ(storage_key, entity_data_map.begin()->first);
    return std::move(entity_data_map.begin()->second);
  }

  void ResetWindows() { window_getter_.ResetWindows(); }

  TestSyncedWindowDelegate* AddWindow(
      int window_id,
      sync_pb::SessionWindow_BrowserType type =
          sync_pb::SessionWindow_BrowserType_TYPE_TABBED) {
    return window_getter_.AddWindow(type,
                                    SessionID::FromSerializedValue(window_id));
  }

  TestSyncedTabDelegate* AddTab(int window_id,
                                const std::string& url,
                                int tab_id = SessionID::NewUnique().id()) {
    TestSyncedTabDelegate* tab =
        window_getter_.AddTab(SessionID::FromSerializedValue(window_id),
                              SessionID::FromSerializedValue(tab_id));
    tab->Navigate(url, base::Time::Now());
    return tab;
  }

  void CloseTab(int tab_id) {
    window_getter_.CloseTab(SessionID::FromSerializedValue(tab_id));
  }

  void SessionRestoreComplete() { window_getter_.SessionRestoreComplete(); }

  MockSyncSessionsClient& mock_sync_sessions_client() {
    return mock_sync_sessions_client_;
  }

  base::MockCallback<base::RepeatingClosure>&
  mock_foreign_session_updated_cb() {
    return mock_foreign_session_updated_cb_;
  }

  SessionSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  syncer::ClientTagBasedModelTypeProcessor* real_processor() {
    return real_processor_.get();
  }

  syncer::ModelTypeStore* underlying_store() { return store_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  const std::unique_ptr<syncer::ModelTypeStore> store_;

  // Dependencies.
  TestingPrefServiceSimple pref_service_;
  SessionSyncPrefs session_sync_prefs_;
  testing::NiceMock<base::MockCallback<base::RepeatingClosure>>
      mock_foreign_session_updated_cb_;
  testing::NiceMock<MockSyncSessionsClient> mock_sync_sessions_client_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  TestSyncedWindowDelegatesGetter window_getter_;
  FaviconCache favicon_cache_;

  std::unique_ptr<SessionSyncBridge> bridge_;
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
};

TEST_F(SessionSyncBridgeTest, ShouldCallModelReadyToSyncWhenSyncEnabled) {
  EXPECT_CALL(mock_processor(), ModelReadyToSync(_)).Times(0);
  InitializeBridge();
  EXPECT_CALL(mock_processor(), ModelReadyToSync(IsEmptyMetadataBatch()));
  StartSyncing();
}

// Test that handling of local events (i.e. propagating the local state to
// sync) does not start while a session restore is in progress.
TEST_F(SessionSyncBridgeTest, ShouldDeferLocalEventDueToSessionRestore) {
  const int kWindowId = 1000001;
  const int kTabId1 = 1000002;
  const int kTabId2 = 1000003;

  // No notifications expected until OnSessionRestoreComplete().
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);

  AddWindow(kWindowId)->SetIsSessionRestoreInProgress(true);
  // Initial tab should be ignored (not exposed to processor) while session
  // restore is in progress.
  AddTab(kWindowId, "http://foo.com/", kTabId1);

  InitializeBridge();
  StartSyncing();
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(
                  _, EntityDataHasSpecifics(MatchesHeader(kLocalSessionTag,
                                                          /*window_ids=*/{},
                                                          /*tab_ids=*/{})))));

  // Create the actual tab, which should be ignored because session restore
  // is in progress.
  AddTab(kWindowId, "http://bar.com/", kTabId2);
  EXPECT_THAT(GetAllData(), SizeIs(1));

  // OnSessionRestoreComplete() should issue three Put() calls, one updating the
  // header and one for each of the two added tabs.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(3);
  SessionRestoreComplete();
  EXPECT_THAT(GetAllData(), SizeIs(3));
}

TEST_F(SessionSyncBridgeTest, ShouldCreateHeaderByDefault) {
  InitializeBridge();

  EXPECT_CALL(mock_processor(), ModelReadyToSync(IsEmptyMetadataBatch()));
  StartSyncing();

  EXPECT_THAT(GetAllData(), SizeIs(1));
}

// Tests that local windows and tabs that exist at the time the bridge is
// started (e.g. after a Chrome restart) are properly exposed via the bridge's
// GetData() and GetAllData() methods, as well as notified via Put().
TEST_F(SessionSyncBridgeTest, ShouldExposeInitialLocalTabsToProcessor) {
  const int kWindowId = 1000001;
  const int kTabId1 = 1000002;
  const int kTabId2 = 1000003;

  AddWindow(kWindowId);
  AddTab(kWindowId, "http://foo.com/", kTabId1);
  AddTab(kWindowId, "http://bar.com/", kTabId2);

  InitializeBridge();

  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);
  const std::string tab_storage_key1 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, 0);
  const std::string tab_storage_key2 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, 1);

  EXPECT_CALL(mock_processor(),
              Put(header_storage_key,
                  EntityDataHasSpecifics(MatchesHeader(
                      kLocalSessionTag, {kWindowId}, {kTabId1, kTabId2})),
                  _));
  EXPECT_CALL(mock_processor(),
              Put(tab_storage_key1,
                  EntityDataHasSpecifics(
                      MatchesTab(kLocalSessionTag, kWindowId, kTabId1,
                                 /*tab_node_id=*/_, {"http://foo.com/"})),
                  _));
  EXPECT_CALL(mock_processor(),
              Put(tab_storage_key2,
                  EntityDataHasSpecifics(
                      MatchesTab(kLocalSessionTag, kWindowId, kTabId2,
                                 /*tab_node_id=*/_, {"http://bar.com/"})),
                  _));

  StartSyncing();

  EXPECT_THAT(GetData(header_storage_key),
              EntityDataHasSpecifics(MatchesHeader(
                  kLocalSessionTag, {kWindowId}, {kTabId1, kTabId2})));
  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(MatchesHeader(
                   kLocalSessionTag, {kWindowId}, {kTabId1, kTabId2}))),
          Pair(tab_storage_key1, EntityDataHasSpecifics(MatchesTab(
                                     kLocalSessionTag, kWindowId, kTabId1,
                                     /*tab_node_id=*/_, {"http://foo.com/"}))),
          Pair(tab_storage_key2,
               EntityDataHasSpecifics(
                   MatchesTab(kLocalSessionTag, kWindowId, kTabId2,
                              /*tab_node_id=*/_, {"http://bar.com/"})))));
}

// Tests that the creation of a new tab while sync is enabled is propagated to:
// 1) The processor, via Put().
// 2) The in-memory representation exposed via GetData().
// 3) The persisted store, exposed via GetAllData().
TEST_F(SessionSyncBridgeTest, ShouldReportLocalTabCreation) {
  const int kWindowId = 1000001;
  const int kTabId1 = 1000002;
  const int kTabId2 = 1000003;

  AddWindow(kWindowId);
  AddTab(kWindowId, "http://foo.com/", kTabId1);

  InitializeBridge();
  StartSyncing();

  ASSERT_THAT(GetAllData(), SizeIs(2));
  EXPECT_CALL(mock_foreign_session_updated_cb(), Run()).Times(0);

  // Expectations for the processor.
  std::string header_storage_key;
  std::string tab_storage_key;
  // Tab creation triggers an update event due to the tab parented notification,
  // so the event handler issues two commits as well (one for tab creation, one
  // for tab update). During the first update, however, the tab is not syncable
  // and is hence skipped.
  testing::Expectation put_transient_header = EXPECT_CALL(
      mock_processor(), Put(_,
                            EntityDataHasSpecifics(MatchesHeader(
                                kLocalSessionTag, {kWindowId}, {kTabId1})),
                            _));
  EXPECT_CALL(mock_processor(),
              Put(_,
                  EntityDataHasSpecifics(MatchesHeader(
                      kLocalSessionTag, {kWindowId}, {kTabId1, kTabId2})),
                  _))
      .After(put_transient_header)
      .WillOnce(WithArg<0>(SaveArg<0>(&header_storage_key)));
  EXPECT_CALL(mock_processor(),
              Put(_,
                  EntityDataHasSpecifics(
                      MatchesTab(kLocalSessionTag, kWindowId, kTabId2,
                                 /*tab_node_id=*/_, {"http://bar.com/"})),
                  _))
      .WillOnce(WithArg<0>(SaveArg<0>(&tab_storage_key)));

  // Create the actual tab, now that we're syncing.
  AddTab(kWindowId, "http://bar.com/", kTabId2);

  ASSERT_THAT(header_storage_key,
              Eq(SessionStore::GetHeaderStorageKey(kLocalSessionTag)));
  ASSERT_THAT(tab_storage_key, Not(IsEmpty()));

  // Verify the bridge's state exposed via the getters.
  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(MatchesHeader(
                   kLocalSessionTag, {kWindowId}, {kTabId1, kTabId2}))),
          Pair(_, EntityDataHasSpecifics(
                      MatchesTab(kLocalSessionTag, kWindowId, kTabId1,
                                 /*tab_node_id=*/_, {"http://foo.com/"}))),
          Pair(tab_storage_key, EntityDataHasSpecifics(MatchesTab(
                                    kLocalSessionTag, kWindowId, kTabId2,
                                    /*tab_node_id=*/_, {"http://bar.com/"})))));
  EXPECT_THAT(GetData(header_storage_key),
              EntityDataHasSpecifics(MatchesHeader(
                  kLocalSessionTag, {kWindowId}, {kTabId1, kTabId2})));
  EXPECT_THAT(GetData(tab_storage_key),
              EntityDataHasSpecifics(
                  MatchesTab(kLocalSessionTag, kWindowId, kTabId2,
                             /*tab_node_id=*/_, {"http://bar.com/"})));
}

TEST_F(SessionSyncBridgeTest, ShouldNotUpdatePlaceholderTabsDuringRestore) {
  const int kWindowId1 = 1000001;
  const int kWindowId2 = 1000002;
  const int kTabId1 = 1000003;
  const int kTabId2 = 1000004;
  // Zero is the first assigned tab node ID.
  const int kTabNodeId1 = 0;
  const int kTabNodeId2 = 1;

  AddWindow(kWindowId1);
  AddTab(kWindowId1, "http://foo.com/", kTabId1);
  AddTab(kWindowId1, "http://bar.com/", kTabId2);

  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);
  const std::string tab_storage_key1 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, kTabNodeId1);
  const std::string tab_storage_key2 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, kTabNodeId2);

  InitializeBridge();
  StartSyncing();

  ASSERT_THAT(GetData(header_storage_key),
              EntityDataHasSpecifics(MatchesHeader(
                  kLocalSessionTag, {kWindowId1}, {kTabId1, kTabId2})));
  ASSERT_THAT(
      GetData(tab_storage_key1),
      EntityDataHasSpecifics(MatchesTab(kLocalSessionTag, kWindowId1, kTabId1,
                                        kTabNodeId1, {"http://foo.com/"})));
  ASSERT_THAT(
      GetData(tab_storage_key2),
      EntityDataHasSpecifics(MatchesTab(kLocalSessionTag, kWindowId1, kTabId2,
                                        kTabNodeId2, {"http://bar.com/"})));

  ShutdownBridge();

  // Override tabs with placeholder tab delegates. Note that, on Android, tab
  // IDs are persisted by session restore across browser restarts.
  PlaceholderTabDelegate placeholder_tab1(
      SessionID::FromSerializedValue(kTabId1));
  PlaceholderTabDelegate placeholder_tab2(
      SessionID::FromSerializedValue(kTabId2));
  ResetWindows();
  TestSyncedWindowDelegate* window = AddWindow(kWindowId2);
  window->OverrideTabAt(0, &placeholder_tab1);
  window->OverrideTabAt(1, &placeholder_tab2);

  // When the bridge gets restarted, we only expect the header to be updated,
  // and placeholder tabs stay unchanged with a stale window ID.
  EXPECT_CALL(mock_processor(),
              Put(header_storage_key,
                  EntityDataHasSpecifics(MatchesHeader(
                      kLocalSessionTag, {kWindowId2}, {kTabId1, kTabId2})),
                  _));

  // Start the bridge again.
  InitializeBridge();
  StartSyncing();

  // Although we haven't notified the processor about the window-ID change, if
  // it hypothetically asked for these entities, the returned entities are
  // up-to-date.
  EXPECT_THAT(GetData(header_storage_key),
              EntityDataHasSpecifics(MatchesHeader(
                  kLocalSessionTag, {kWindowId2}, {kTabId1, kTabId2})));
  EXPECT_THAT(
      GetData(tab_storage_key1),
      EntityDataHasSpecifics(MatchesTab(kLocalSessionTag, kWindowId2, kTabId1,
                                        kTabNodeId1, {"http://foo.com/"})));
  EXPECT_THAT(
      GetData(tab_storage_key2),
      EntityDataHasSpecifics(MatchesTab(kLocalSessionTag, kWindowId2, kTabId2,
                                        kTabNodeId2, {"http://bar.com/"})));

  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(MatchesHeader(
                   kLocalSessionTag, {kWindowId2}, {kTabId1, kTabId2}))),
          Pair(tab_storage_key1, EntityDataHasSpecifics(MatchesTab(
                                     kLocalSessionTag, kWindowId2, kTabId1,
                                     kTabNodeId1, {"http://foo.com/"}))),
          Pair(tab_storage_key2, EntityDataHasSpecifics(MatchesTab(
                                     kLocalSessionTag, kWindowId2, kTabId2,
                                     kTabNodeId2, {"http://bar.com/"})))));
}

TEST_F(SessionSyncBridgeTest,
       ShouldIgnoreUnsyncablePlaceholderTabDuringRestore) {
  const int kWindowId1 = 1000001;
  const int kWindowId2 = 1000002;
  const int kTabId1 = 1000002;
  const int kTabId2 = 1000003;
  // Zero is the first assigned tab node ID.
  const int kTabNodeId1 = 0;

  AddWindow(kWindowId1);
  AddTab(kWindowId1, "http://foo.com/", kTabId1);
  // Tab 2 is unsyncable because of the URL scheme.
  AddTab(kWindowId1, "about:blank", kTabId2);

  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);
  const std::string tab_storage_key1 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, kTabNodeId1);

  InitializeBridge();
  StartSyncing();

  ASSERT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(
                   MatchesHeader(kLocalSessionTag, {kWindowId1}, {kTabId1}))),
          Pair(tab_storage_key1, EntityDataHasSpecifics(MatchesTab(
                                     kLocalSessionTag, kWindowId1, kTabId1,
                                     kTabNodeId1, {"http://foo.com/"})))));

  ShutdownBridge();

  // Override tabs with placeholder tab delegates. Note that, on Android, tab
  // IDs are persisted by session restore across browser restarts.
  PlaceholderTabDelegate placeholder_tab1(
      SessionID::FromSerializedValue(kTabId1));
  PlaceholderTabDelegate placeholder_tab2(
      SessionID::FromSerializedValue(kTabId2));
  ResetWindows();
  TestSyncedWindowDelegate* window = AddWindow(kWindowId2);
  window->OverrideTabAt(0, &placeholder_tab1);
  window->OverrideTabAt(1, &placeholder_tab2);

  // Start the bridge again.
  InitializeBridge();
  StartSyncing();

  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(
                   MatchesHeader(kLocalSessionTag, {kWindowId2}, {kTabId1}))),
          Pair(tab_storage_key1, EntityDataHasSpecifics(MatchesTab(
                                     kLocalSessionTag, kWindowId2, kTabId1,
                                     kTabNodeId1, {"http://foo.com/"})))));
}

// Ensure that tabbed windows from a previous session are preserved if no
// windows are present on startup.
TEST_F(SessionSyncBridgeTest, ShouldRestoreTabbedDataIfNoWindowsDuringStartup) {
  const int kWindowId1 = 1000001;
  const int kWindowId2 = 1000002;
  const int kTabNodeId = 0;

  AddWindow(kWindowId1);
  TestSyncedTabDelegate* tab = AddTab(kWindowId1, "http://foo.com/");

  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);
  const std::string tab_storage_key =
      SessionStore::GetTabStorageKey(kLocalSessionTag, kTabNodeId);

  InitializeBridge();
  StartSyncing();

  ASSERT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(MatchesHeader(kLocalSessionTag, _, _))),
          Pair(tab_storage_key,
               EntityDataHasSpecifics(MatchesTab(
                   kLocalSessionTag, _, _, kTabNodeId, {"http://foo.com/"})))));

  ShutdownBridge();

  // Start the bridge with no local windows/tabs.
  ResetWindows();
  InitializeBridge();
  StartSyncing();

  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(MatchesHeader(kLocalSessionTag, _, _))),
          Pair(tab_storage_key,
               EntityDataHasSpecifics(MatchesTab(
                   kLocalSessionTag, _, _, kTabNodeId, {"http://foo.com/"})))));

  // Now actually resurrect the native data, which will end up having different
  // native ids, but the tab has the same sync id as before.
  EXPECT_CALL(
      mock_processor(),
      Put(header_storage_key,
          EntityDataHasSpecifics(MatchesHeader(kLocalSessionTag, _, _)), _));
  EXPECT_CALL(mock_processor(),
              Put(tab_storage_key,
                  EntityDataHasSpecifics(MatchesTab(
                      kLocalSessionTag, /*window_id=*/_, /*tab_id=*/_,
                      kTabNodeId, {"http://foo.com/", "http://bar.com/"})),
                  _));
  AddWindow(kWindowId2)->OverrideTabAt(0, tab);
  tab->Navigate("http://bar.com/");
}

// Ensure that tabbed windows from a previous session are preserved if only
// a custom tab is present at startup.
TEST_F(SessionSyncBridgeTest, ShouldPreserveTabbedDataIfCustomTabOnlyFound) {
  const int kWindowId1 = 1000001;
  const int kWindowId2 = 1000002;

  AddWindow(kWindowId1);
  AddTab(kWindowId1, "http://foo.com/");

  InitializeBridge();
  StartSyncing();

  ASSERT_THAT(GetAllData(),
              UnorderedElementsAre(
                  Pair(_, EntityDataHasSpecifics(
                              MatchesHeader(kLocalSessionTag, _, _))),
                  Pair(_, EntityDataHasSpecifics(MatchesTab(
                              kLocalSessionTag, _, _,
                              /*tab_node_id=*/0, {"http://foo.com/"})))));

  ShutdownBridge();

  // Start the bridge with only a custom tab open.
  ResetWindows();
  AddWindow(kWindowId2, sync_pb::SessionWindow_BrowserType_TYPE_CUSTOM_TAB);
  AddTab(kWindowId2, "http://bar.com/");
  InitializeBridge();
  StartSyncing();

  // The previous session should be preserved, together with the new custom tab.
  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(_,
               EntityDataHasSpecifics(MatchesHeader(kLocalSessionTag, _, _))),
          Pair(_, EntityDataHasSpecifics(MatchesTab(kLocalSessionTag, _, _,
                                                    /*tab_node_id=*/0,
                                                    {"http://foo.com/"}))),
          Pair(_, EntityDataHasSpecifics(MatchesTab(kLocalSessionTag, _, _,
                                                    /*tab_node_id=*/1,
                                                    {"http://bar.com/"})))));
}

// Ensure that tabbed windows from a previous session are preserved and combined
// with a custom tab that was newly found during startup.
TEST_F(SessionSyncBridgeTest, ShouldPreserveTabbedDataIfNewCustomTabAlsoFound) {
  const int kWindowId1 = 1000001;
  const int kWindowId2 = 1000002;
  const int kTabId1 = 1000003;
  const int kTabId2 = 1000004;

  AddWindow(kWindowId1);
  AddTab(kWindowId1, "http://foo.com/", kTabId1);

  InitializeBridge();
  StartSyncing();

  ASSERT_THAT(GetAllData(),
              UnorderedElementsAre(
                  Pair(_, EntityDataHasSpecifics(MatchesHeader(
                              kLocalSessionTag, {kWindowId1}, {kTabId1}))),
                  Pair(_, EntityDataHasSpecifics(MatchesTab(
                              kLocalSessionTag, kWindowId1, kTabId1,
                              /*tab_node_id=*/0, {"http://foo.com/"})))));

  ShutdownBridge();

  // Start the bridge with an additional local custom tab.
  AddWindow(kWindowId2, sync_pb::SessionWindow_BrowserType_TYPE_CUSTOM_TAB);
  AddTab(kWindowId2, "http://bar.com/", kTabId2);
  InitializeBridge();
  StartSyncing();

  EXPECT_THAT(GetAllData(),
              UnorderedElementsAre(
                  Pair(_, EntityDataHasSpecifics(MatchesHeader(
                              kLocalSessionTag, {kWindowId1, kWindowId2},
                              {kTabId1, kTabId2}))),
                  Pair(_, EntityDataHasSpecifics(MatchesTab(
                              kLocalSessionTag, kWindowId1, kTabId1,
                              /*tab_node_id=*/0, {"http://foo.com/"}))),
                  Pair(_, EntityDataHasSpecifics(MatchesTab(
                              kLocalSessionTag, kWindowId2, kTabId2,
                              /*tab_node_id=*/1, {"http://bar.com/"})))));
}

// Ensure that, in a scenario without prior sync data, encountering a custom
// tab only (no tabbed window) starts syncing that tab.
TEST_F(SessionSyncBridgeTest, ShouldAssociateIfCustomTabOnlyOnStartup) {
  const int kWindowId = 1000001;
  const int kTabId = 1000002;

  AddWindow(kWindowId, sync_pb::SessionWindow_BrowserType_TYPE_CUSTOM_TAB);
  AddTab(kWindowId, "http://foo.com/", kTabId);

  InitializeBridge();
  StartSyncing();

  EXPECT_THAT(GetAllData(),
              UnorderedElementsAre(
                  Pair(_, EntityDataHasSpecifics(MatchesHeader(
                              kLocalSessionTag, {kWindowId}, {kTabId}))),
                  Pair(_, EntityDataHasSpecifics(MatchesTab(
                              kLocalSessionTag, kWindowId, kTabId,
                              /*tab_node_id=*/0, {"http://foo.com/"})))));
}

// Ensure that all tabs are exposed in a scenario where only a custom tab
// (without tabbed windows) was present during startup, and later tabbed windows
// appear (browser started).
TEST_F(SessionSyncBridgeTest, ShouldExposeTabbedWindowAfterCustomTabOnly) {
  const int kWindowId1 = 1000001;
  const int kWindowId2 = 1000002;
  const int kTabId1 = 1000003;
  const int kTabId2 = 1000004;

  AddWindow(kWindowId1, sync_pb::SessionWindow_BrowserType_TYPE_CUSTOM_TAB);
  AddTab(kWindowId1, "http://foo.com/", kTabId1);

  InitializeBridge();
  StartSyncing();

  ASSERT_THAT(GetAllData(),
              UnorderedElementsAre(
                  Pair(_, EntityDataHasSpecifics(MatchesHeader(
                              kLocalSessionTag, {kWindowId1}, {kTabId1}))),
                  Pair(_, EntityDataHasSpecifics(MatchesTab(
                              kLocalSessionTag, kWindowId1, kTabId1,
                              /*tab_node_id=*/0, {"http://foo.com/"})))));

  // Load the actual tabbed window, now that we're syncing.
  AddWindow(kWindowId2);
  AddTab(kWindowId2, "http://bar.com/", kTabId2);

  // The local change should be created and tracked correctly.
  EXPECT_THAT(GetAllData(),
              UnorderedElementsAre(
                  Pair(_, EntityDataHasSpecifics(MatchesHeader(
                              kLocalSessionTag, {kWindowId1, kWindowId2},
                              {kTabId1, kTabId2}))),
                  Pair(_, EntityDataHasSpecifics(MatchesTab(
                              kLocalSessionTag, kWindowId1, kTabId1,
                              /*tab_node_id=*/0, {"http://foo.com/"}))),
                  Pair(_, EntityDataHasSpecifics(MatchesTab(
                              kLocalSessionTag, kWindowId2, kTabId2,
                              /*tab_node_id=*/1, {"http://bar.com/"})))));
}

TEST_F(SessionSyncBridgeTest, ShouldRecycleTabNodeAfterCommitCompleted) {
  const int kWindowId = 1000001;
  const int kTabId1 = 1000003;
  const int kTabId2 = 1000004;
  const int kTabId3 = 1000005;
  const int kTabId4 = 1000006;
  // Zero is the first assigned tab node ID.
  const int kTabNodeId1 = 0;
  const int kTabNodeId2 = 1;
  const int kTabNodeId3 = 2;

  AddWindow(kWindowId);
  TestSyncedTabDelegate* tab1 = AddTab(kWindowId, "http://foo.com/", kTabId1);

  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);
  const std::string tab_storage_key1 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, kTabNodeId1);
  const std::string tab_storage_key2 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, kTabNodeId2);
  const std::string tab_storage_key3 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, kTabNodeId3);
  const std::string tab_client_tag1 =
      SessionStore::GetTabClientTagForTest(kLocalSessionTag, kTabNodeId1);
  const std::string tab_client_tag2 =
      SessionStore::GetTabClientTagForTest(kLocalSessionTag, kTabNodeId2);
  const std::string tab_client_tag3 =
      SessionStore::GetTabClientTagForTest(kLocalSessionTag, kTabNodeId3);

  InitializeBridge();
  StartSyncing();

  // Mimic a commit completing for the initial sync.
  ASSERT_TRUE(real_processor()->HasLocalChangesForTest());
  sync_pb::ModelTypeState state;
  state.set_initial_sync_done(true);
  real_processor()->OnCommitCompleted(state,
                                      {CreateSuccessResponse(kLocalSessionTag),
                                       CreateSuccessResponse(tab_client_tag1)});
  ASSERT_FALSE(real_processor()->HasLocalChangesForTest());

  // Open a second tab.
  AddTab(kWindowId, "http://bar.com/", kTabId2);
  ASSERT_TRUE(real_processor()->HasLocalChangesForTest());

  // Close |kTabId2| and force reassociation by navigating in the remaining open
  // tab, leading to a freed tab entity. However, while there are pending
  // changes to commit, the entity shouldn't be deleted (to prevent history
  // loss).
  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);
  CloseTab(kTabId2);
  tab1->Navigate("http://foo2.com/");
  EXPECT_TRUE(real_processor()->HasLocalChangesForTest());

  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(
                   MatchesHeader(kLocalSessionTag, {kWindowId}, {kTabId1}))),
          Pair(tab_storage_key1,
               EntityDataHasSpecifics(
                   MatchesTab(kLocalSessionTag, kWindowId, kTabId1, kTabNodeId1,
                              {"http://foo.com/", "http://foo2.com/"}))),
          Pair(tab_storage_key2, EntityDataHasSpecifics(MatchesTab(
                                     kLocalSessionTag, kWindowId, kTabId2,
                                     kTabNodeId2, {"http://bar.com/"})))));

  // If a new tab is opened, the entity with unsynced changes should not be
  // recycled.
  AddTab(kWindowId, "http://baz.com/", kTabId3);
  EXPECT_THAT(GetAllData(), UnorderedElementsAre(Pair(header_storage_key, _),
                                                 Pair(tab_storage_key1, _),
                                                 Pair(tab_storage_key2, _),
                                                 Pair(tab_storage_key3, _)));

  // Completing the commit for the previously closed tab should issue a
  // deletion. For that to trigger, we need to trigger the next association,
  // which we do by navigating in one of the open tabs.
  EXPECT_CALL(mock_processor(), Delete(tab_storage_key2, _));
  real_processor()->OnCommitCompleted(state,
                                      {CreateSuccessResponse(tab_client_tag2)});
  tab1->Navigate("http://foo3.com/");
  EXPECT_THAT(GetAllData(), UnorderedElementsAre(Pair(header_storage_key, _),
                                                 Pair(tab_storage_key1, _),
                                                 Pair(tab_storage_key3, _)));

  // If yet anothertab is opened, the entity for the closed tab should be
  // recycled.
  AddTab(kWindowId, "http://qux.com/", kTabId4);
  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key, _), Pair(tab_storage_key1, _),
          Pair(tab_storage_key2, EntityDataHasSpecifics(MatchesTab(
                                     kLocalSessionTag, kWindowId, kTabId4,
                                     kTabNodeId2, {"http://qux.com/"}))),
          Pair(tab_storage_key3, _)));
}

TEST_F(SessionSyncBridgeTest, ShouldRestoreLocalSessionWithFreedTab) {
  const int kWindowId1 = 1000001;
  const int kWindowId2 = 1000002;
  const int kTabId1 = 1000003;
  const int kTabId2 = 1000004;
  const int kTabId3 = 1000005;
  // Zero is the first assigned tab node ID.
  const int kTabNodeId1 = 0;
  const int kTabNodeId2 = 1;
  const int kTabNodeId3 = 2;

  AddWindow(kWindowId1);
  TestSyncedTabDelegate* tab1 = AddTab(kWindowId1, "http://foo.com/", kTabId1);
  AddTab(kWindowId1, "http://bar.com/", kTabId2);

  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);
  const std::string tab_storage_key1 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, kTabNodeId1);
  const std::string tab_storage_key2 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, kTabNodeId2);
  const std::string tab_storage_key3 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, kTabNodeId3);

  InitializeBridge();
  StartSyncing();

  ASSERT_THAT(GetData(header_storage_key),
              EntityDataHasSpecifics(MatchesHeader(
                  kLocalSessionTag, {kWindowId1}, {kTabId1, kTabId2})));

  // Close |kTabId2| and force reassociation by navigating in the remaining open
  // tab, leading to a freed tab entity.
  CloseTab(kTabId2);
  tab1->Navigate("http://foo2.com/");

  ASSERT_THAT(GetData(header_storage_key),
              EntityDataHasSpecifics(
                  MatchesHeader(kLocalSessionTag, {kWindowId1}, {kTabId1})));

  ShutdownBridge();
  ResetWindows();

  // The browser gets restarted with a new initial tab, for example because the
  // user chose "Continue where you left off".
  AddWindow(kWindowId2);
  AddTab(kWindowId2, "http://baz.com/", kTabId3);

  // Start the bridge again.
  InitializeBridge();
  StartSyncing();

  // Two tab nodes should be free at this point, because both tabs have been
  // closed. However, they are also unsynced (the commit hasn't completed),
  // which prevents their recycling, so a new tab node should be created.
  ASSERT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(
                   MatchesHeader(kLocalSessionTag, {kWindowId2}, {kTabId3}))),
          Pair(tab_storage_key1,
               EntityDataHasSpecifics(MatchesTab(
                   kLocalSessionTag, kWindowId1, kTabId1, kTabNodeId1,
                   {"http://foo.com/", "http://foo2.com/"}))),
          Pair(tab_storage_key2, EntityDataHasSpecifics(MatchesTab(
                                     kLocalSessionTag, kWindowId1, kTabId2,
                                     kTabNodeId2, {"http://bar.com/"}))),
          Pair(tab_storage_key3, EntityDataHasSpecifics(MatchesTab(
                                     kLocalSessionTag, kWindowId2, kTabId3,
                                     kTabNodeId3, {"http://baz.com/"})))));
}

TEST_F(SessionSyncBridgeTest, ShouldDisableSyncAndReenable) {
  const int kWindowId = 1000001;
  const int kTabId = 1000002;

  AddWindow(kWindowId);
  AddTab(kWindowId, "http://foo.com/", kTabId);

  InitializeBridge();
  StartSyncing();

  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);
  ASSERT_THAT(GetData(header_storage_key),
              EntityDataHasSpecifics(
                  MatchesHeader(kLocalSessionTag, {kWindowId}, {kTabId})));
  ASSERT_THAT(GetAllData(), Not(IsEmpty()));

  EXPECT_CALL(mock_processor(), ModelReadyToSync(_)).Times(0);
  real_processor()->OnSyncStopping(syncer::CLEAR_METADATA);

  StartSyncing();
  ASSERT_THAT(GetData(header_storage_key),
              EntityDataHasSpecifics(
                  MatchesHeader(kLocalSessionTag, {kWindowId}, {kTabId})));
}

// Starting sync with no local data should just store the foreign entities in
// the store and expose them via OpenTabsUIDelegate.
TEST_F(SessionSyncBridgeTest, ShouldMergeForeignSession) {
  const std::string kForeignSessionTag = "foreignsessiontag";
  const int kForeignWindowId = 2000001;
  const int kForeignTabId = 2000002;
  const int kForeignTabNodeId = 2003;

  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, _, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  InitializeBridge();

  const sync_pb::SessionSpecifics foreign_header =
      CreateHeaderSpecificsWithOneTab(kForeignSessionTag, kForeignWindowId,
                                      kForeignTabId);
  const sync_pb::SessionSpecifics foreign_tab =
      CreateTabSpecifics(kForeignSessionTag, kForeignWindowId, kForeignTabId,
                         kForeignTabNodeId, "http://baz.com/");

  EXPECT_CALL(
      mock_processor(),
      Put(_, EntityDataHasSpecifics(MatchesHeader(kLocalSessionTag, _, _)), _));
  EXPECT_CALL(mock_foreign_session_updated_cb(), Run());
  StartSyncing({foreign_header, foreign_tab});

  std::vector<const SyncedSession*> foreign_sessions;
  EXPECT_TRUE(bridge()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  EXPECT_THAT(foreign_sessions,
              ElementsAre(MatchesSyncedSession(
                  kForeignSessionTag,
                  {{kForeignWindowId, std::vector<int>{kForeignTabId}}})));
}

TEST_F(SessionSyncBridgeTest, ShouldNotExposeForeignHeaderWithoutTabs) {
  const std::string kForeignSessionTag = "foreignsessiontag";
  const int kForeignWindowId = 2000001;
  const int kForeignTabId = 2000002;

  EXPECT_CALL(mock_processor(), UpdateStorageKey(_, _, _)).Times(0);
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  InitializeBridge();

  const sync_pb::SessionSpecifics foreign_header =
      CreateHeaderSpecificsWithOneTab(kForeignSessionTag, kForeignWindowId,
                                      kForeignTabId);
  const std::string foreign_header_storage_key =
      SessionStore::GetHeaderStorageKey(kForeignSessionTag);

  EXPECT_CALL(
      mock_processor(),
      Put(_, EntityDataHasSpecifics(MatchesHeader(kLocalSessionTag, _, _)), _));

  StartSyncing({foreign_header});
  ASSERT_THAT(GetData(foreign_header_storage_key), NotNull());

  std::vector<const SyncedSession*> foreign_sessions;
  EXPECT_FALSE(bridge()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));

  // Restart bridge to verify the state doesn't change.
  ShutdownBridge();
  InitializeBridge();
  StartSyncing();
  ASSERT_THAT(GetData(foreign_header_storage_key), NotNull());

  EXPECT_FALSE(bridge()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
}

// Regression test for crbug.com/837517: Ensure that the bridge doesn't crash
// and closed foreign tabs (|kForeignTabId2| in the test) are not exposed after
// restarting the browser.
TEST_F(SessionSyncBridgeTest, ShouldNotExposeClosedTabsAfterRestart) {
  const std::string kForeignSessionTag = "foreignsessiontag";
  const int kForeignWindowId = 2000001;
  const int kForeignTabId1 = 2000002;
  const int kForeignTabId2 = 2000003;
  const int kForeignTabNodeId1 = 2004;
  const int kForeignTabNodeId2 = 2005;

  // The header only lists a single tab |kForeignTabId1|, which becomes a mapped
  // tab.
  const sync_pb::SessionSpecifics foreign_header =
      CreateHeaderSpecificsWithOneTab(kForeignSessionTag, kForeignWindowId,
                                      kForeignTabId1);
  const sync_pb::SessionSpecifics foreign_tab1 =
      CreateTabSpecifics(kForeignSessionTag, kForeignWindowId, kForeignTabId1,
                         kForeignTabNodeId1, "http://foo.com/");
  // |kForeignTabId2| is not present in the header, leading to an unmapped tab.
  const sync_pb::SessionSpecifics foreign_tab2 =
      CreateTabSpecifics(kForeignSessionTag, kForeignWindowId, kForeignTabId2,
                         kForeignTabNodeId2, "http://bar.com/");

  InitializeBridge();
  StartSyncing({foreign_header, foreign_tab1, foreign_tab2});

  const std::string local_header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);
  const std::string foreign_header_storage_key =
      SessionStore::GetHeaderStorageKey(kForeignSessionTag);
  const std::string foreign_tab_storage_key1 =
      SessionStore::GetTabStorageKey(kForeignSessionTag, kForeignTabNodeId1);
  const std::string foreign_tab_storage_key2 =
      SessionStore::GetTabStorageKey(kForeignSessionTag, kForeignTabNodeId2);

  ASSERT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(local_header_storage_key, _),
          Pair(foreign_header_storage_key,
               EntityDataHasSpecifics(MatchesHeader(
                   kForeignSessionTag, {kForeignWindowId}, {kForeignTabId1}))),
          Pair(foreign_tab_storage_key1,
               EntityDataHasSpecifics(MatchesTab(
                   kForeignSessionTag, kForeignWindowId, kForeignTabId1,
                   kForeignTabNodeId1, {"http://foo.com/"}))),
          Pair(foreign_tab_storage_key2,
               EntityDataHasSpecifics(MatchesTab(
                   kForeignSessionTag, kForeignWindowId, kForeignTabId2,
                   kForeignTabNodeId2, {"http://bar.com/"})))));

  // Mimic a browser restart, which should restore the very same state (and not
  // crash!).
  ShutdownBridge();
  InitializeBridge();
  StartSyncing();

  EXPECT_THAT(GetAllData(),
              UnorderedElementsAre(Pair(local_header_storage_key, _),
                                   Pair(foreign_header_storage_key, _),
                                   Pair(foreign_tab_storage_key1, _),
                                   Pair(foreign_tab_storage_key2, _)));
}

TEST_F(SessionSyncBridgeTest, ShouldHandleRemoteDeletion) {
  const std::string kForeignSessionTag = "foreignsessiontag";
  const int kForeignWindowId = 2000001;
  const int kForeignTabId = 2000002;
  const int kForeignTabNodeId = 2003;

  InitializeBridge();

  const sync_pb::SessionSpecifics foreign_header =
      CreateHeaderSpecificsWithOneTab(kForeignSessionTag, kForeignWindowId,
                                      kForeignTabId);
  const sync_pb::SessionSpecifics foreign_tab =
      CreateTabSpecifics(kForeignSessionTag, kForeignWindowId, kForeignTabId,
                         kForeignTabNodeId, "http://baz.com/");
  StartSyncing({foreign_header, foreign_tab});

  sync_pb::ModelTypeState state;
  state.set_initial_sync_done(true);

  // Mimic receiving a commit ack for the local header entity, to later be able
  // to verify HasLocalChangesForTest() without interferences from the local
  // session.
  ASSERT_TRUE(real_processor()->HasLocalChangesForTest());
  real_processor()->OnCommitCompleted(
      state, {CreateSuccessResponse(kLocalSessionTag)});
  ASSERT_FALSE(real_processor()->HasLocalChangesForTest());

  const sessions::SessionTab* foreign_session_tab = nullptr;
  ASSERT_TRUE(bridge()->GetOpenTabsUIDelegate()->GetForeignTab(
      kForeignSessionTag, SessionID::FromSerializedValue(kForeignTabId),
      &foreign_session_tab));
  ASSERT_THAT(foreign_session_tab, NotNull());
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(bridge()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_THAT(foreign_sessions,
              ElementsAre(MatchesSyncedSession(
                  kForeignSessionTag,
                  {{kForeignWindowId, std::vector<int>{kForeignTabId}}})));
  ASSERT_TRUE(real_processor()->IsTrackingMetadata());
  ASSERT_TRUE(real_processor()->IsTrackingEntityForTest(
      SessionStore::GetHeaderStorageKey(kForeignSessionTag)));
  ASSERT_TRUE(real_processor()->IsTrackingEntityForTest(
      SessionStore::GetTabStorageKey(kForeignSessionTag, kForeignTabNodeId)));
  ASSERT_FALSE(real_processor()->HasLocalChangesForTest());

  // Mimic receiving a remote deletion of the foreign session.
  EXPECT_CALL(mock_foreign_session_updated_cb(), Run());
  syncer::UpdateResponseDataList updates;
  updates.push_back(
      CreateTombstone(SessionStore::GetClientTag(foreign_header)));
  real_processor()->OnUpdateReceived(state, std::move(updates));

  foreign_session_tab = nullptr;
  EXPECT_FALSE(bridge()->GetOpenTabsUIDelegate()->GetForeignTab(
      kForeignSessionTag, SessionID::FromSerializedValue(kForeignTabId),
      &foreign_session_tab));

  EXPECT_FALSE(bridge()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));

  EXPECT_FALSE(real_processor()->HasLocalChangesForTest());

  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kForeignSessionTag);
  const std::string tab_storage_key =
      SessionStore::GetTabStorageKey(kForeignSessionTag, kForeignTabNodeId);

  EXPECT_FALSE(real_processor()->IsTrackingEntityForTest(header_storage_key));
  EXPECT_FALSE(real_processor()->IsTrackingEntityForTest(tab_storage_key));

  // Verify that both entities have been deleted from storage.
  {
    base::RunLoop loop;
    underlying_store()->ReadData(
        {header_storage_key, tab_storage_key},
        base::BindLambdaForTesting(
            [&](const base::Optional<syncer::ModelError>& error,
                std::unique_ptr<syncer::ModelTypeStore::RecordList>
                    data_records,
                std::unique_ptr<syncer::ModelTypeStore::IdList>
                    missing_id_list) {
              EXPECT_THAT(data_records, Pointee(IsEmpty()));
              EXPECT_THAT(
                  missing_id_list,
                  Pointee(ElementsAre(header_storage_key, tab_storage_key)));
              loop.Quit();
            }));
    loop.Run();
  }

  // Verify that the sync metadata for both entities have been deleted too.
  {
    base::RunLoop loop;
    underlying_store()->ReadAllMetadata(base::BindLambdaForTesting(
        [&](const base::Optional<syncer::ModelError>& error,
            std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
          syncer::EntityMetadataMap entity_metadata_map =
              metadata_batch->TakeAllMetadata();
          EXPECT_EQ(0U, entity_metadata_map.count(header_storage_key));
          EXPECT_EQ(0U, entity_metadata_map.count(tab_storage_key));
          loop.Quit();
        }));
    loop.Run();
  }
}

TEST_F(SessionSyncBridgeTest, ShouldIgnoreRemoteDeletionOfLocalTab) {
  const int kWindowId1 = 1000001;
  const int kTabId1 = 1000002;
  const int kTabNodeId1 = 0;

  AddWindow(kWindowId1);
  AddTab(kWindowId1, "http://foo.com/", kTabId1);

  InitializeBridge();
  StartSyncing();

  const std::string header_storage_key =
      SessionStore::GetHeaderStorageKey(kLocalSessionTag);
  const std::string tab_storage_key1 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, kTabNodeId1);
  const std::string tab_client_tag1 =
      SessionStore::GetTabClientTagForTest(kLocalSessionTag, kTabNodeId1);

  ASSERT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(MatchesHeader(kLocalSessionTag, _, _))),
          Pair(tab_storage_key1, EntityDataHasSpecifics(MatchesTab(
                                     kLocalSessionTag, kWindowId1, kTabId1,
                                     kTabNodeId1, {"http://foo.com/"})))));
  ASSERT_TRUE(real_processor()->IsTrackingMetadata());
  ASSERT_TRUE(real_processor()->HasLocalChangesForTest());

  // Mimic receiving a commit ack for both the tab and the header entity,
  // because otherwise it will be treated as conflict, and then local wins.
  sync_pb::ModelTypeState state;
  state.set_initial_sync_done(true);
  real_processor()->OnCommitCompleted(
      state, {CreateSuccessResponse(tab_client_tag1),
              CreateSuccessResponse(kLocalSessionTag)});
  ASSERT_FALSE(real_processor()->HasLocalChangesForTest());

  // Mimic receiving a remote deletion of both entities.
  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateTombstone(kLocalSessionTag));
  updates.push_back(CreateTombstone(tab_client_tag1));
  real_processor()->OnUpdateReceived(state, std::move(updates));

  // State should remain unchanged (deletions ignored).
  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(MatchesHeader(kLocalSessionTag, _, _))),
          Pair(tab_storage_key1, EntityDataHasSpecifics(MatchesTab(
                                     kLocalSessionTag, kWindowId1, kTabId1,
                                     kTabNodeId1, {"http://foo.com/"})))));

  // Creating a new tab locally should trigger Put() calls for *all* entities
  // (because the local data was out of sync).
  const int kWindowId2 = 2000001;
  const int kTabId2 = 2000002;
  const int kTabNodeId2 = 1;

  const std::string tab_storage_key2 =
      SessionStore::GetTabStorageKey(kLocalSessionTag, kTabNodeId2);

  // Window creation already triggers a header update, which will be overriden
  // later below.
  testing::Expectation put_transient_header =
      EXPECT_CALL(mock_processor(), Put(header_storage_key, _, _));
  AddWindow(kWindowId2);

  // In the current implementation, some of the updates are reported to the
  // processor twice, but that's OK because the processor can detect it.
  EXPECT_CALL(mock_processor(),
              Put(header_storage_key,
                  EntityDataHasSpecifics(MatchesHeader(
                      kLocalSessionTag, ElementsAre(kWindowId1, kWindowId2),
                      ElementsAre(kTabId1, kTabId2))),
                  _))
      .Times(2)
      .After(put_transient_header);
  EXPECT_CALL(mock_processor(), Put(tab_storage_key1,
                                    EntityDataHasSpecifics(MatchesTab(
                                        kLocalSessionTag, kWindowId1, kTabId1,
                                        kTabNodeId1, {"http://foo.com/"})),
                                    _));
  EXPECT_CALL(mock_processor(), Put(tab_storage_key2,
                                    EntityDataHasSpecifics(MatchesTab(
                                        kLocalSessionTag, kWindowId2, kTabId2,
                                        kTabNodeId2, {"http://bar.com/"})),
                                    _))
      .Times(2);

  AddTab(kWindowId2, "http://bar.com/", kTabId2);

  EXPECT_THAT(
      GetAllData(),
      UnorderedElementsAre(
          Pair(header_storage_key,
               EntityDataHasSpecifics(MatchesHeader(
                   kLocalSessionTag, ElementsAre(kWindowId1, kWindowId2),
                   ElementsAre(kTabId1, kTabId2)))),
          Pair(tab_storage_key1,
               EntityDataHasSpecifics(
                   MatchesTab(kLocalSessionTag, /*window_id=*/_, /*tab_id=*/_,
                              kTabNodeId1, {"http://foo.com/"}))),
          Pair(tab_storage_key2, EntityDataHasSpecifics(MatchesTab(
                                     kLocalSessionTag, kWindowId2, kTabId2,
                                     kTabNodeId2, {"http://bar.com/"})))));

  // Run until idle because PostTask() is used to invoke ResubmitLocalSession().
  base::RunLoop().RunUntilIdle();
}

// Verifies that a foreign session can be deleted by the user from the history
// UI (via OpenTabsUIDelegate).
TEST_F(SessionSyncBridgeTest, ShouldDeleteForeignSessionFromUI) {
  const std::string kForeignSessionTag = "foreignsessiontag";
  const int kForeignWindowId = 2000001;
  const int kForeignTabId = 2000002;
  const int kForeignTabNodeId = 2003;

  InitializeBridge();

  const sync_pb::SessionSpecifics foreign_header =
      CreateHeaderSpecificsWithOneTab(kForeignSessionTag, kForeignWindowId,
                                      kForeignTabId);
  const sync_pb::SessionSpecifics foreign_tab =
      CreateTabSpecifics(kForeignSessionTag, kForeignWindowId, kForeignTabId,
                         kForeignTabNodeId, "http://baz.com/");
  StartSyncing({foreign_header, foreign_tab});

  const std::string foreign_header_storage_key =
      SessionStore::GetHeaderStorageKey(kForeignSessionTag);
  const std::string foreign_tab_storage_key =
      SessionStore::GetTabStorageKey(kForeignSessionTag, kForeignTabNodeId);

  // Test fixture expects the two foreign entities in the model as well as the
  // underlying store.
  ASSERT_THAT(GetData(foreign_header_storage_key), NotNull());
  ASSERT_THAT(GetData(foreign_tab_storage_key), NotNull());

  const sessions::SessionTab* foreign_session_tab = nullptr;
  ASSERT_TRUE(bridge()->GetOpenTabsUIDelegate()->GetForeignTab(
      kForeignSessionTag, SessionID::FromSerializedValue(kForeignTabId),
      &foreign_session_tab));
  ASSERT_THAT(foreign_session_tab, NotNull());
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(bridge()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_THAT(foreign_sessions,
              ElementsAre(MatchesSyncedSession(
                  kForeignSessionTag,
                  {{kForeignWindowId, std::vector<int>{kForeignTabId}}})));
  ASSERT_TRUE(real_processor()->IsTrackingMetadata());

  // Mimic the user requesting a session deletion from the UI.
  EXPECT_CALL(mock_processor(), Delete(foreign_header_storage_key, _));
  EXPECT_CALL(mock_processor(), Delete(foreign_tab_storage_key, _));
  EXPECT_CALL(mock_foreign_session_updated_cb(), Run());
  bridge()->GetOpenTabsUIDelegate()->DeleteForeignSession(kForeignSessionTag);

  // Verify what gets exposed to the UI.
  foreign_session_tab = nullptr;
  EXPECT_FALSE(bridge()->GetOpenTabsUIDelegate()->GetForeignTab(
      kForeignSessionTag, SessionID::FromSerializedValue(kForeignTabId),
      &foreign_session_tab));
  EXPECT_FALSE(bridge()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));

  // Verify store.
  EXPECT_THAT(GetData(foreign_header_storage_key), IsNull());
  EXPECT_THAT(GetData(foreign_tab_storage_key), IsNull());
}

// Verifies that attempts to delete the local session from the UI are ignored,
// although the UI sholdn't really be offering that option.
TEST_F(SessionSyncBridgeTest, ShouldIgnoreLocalSessionDeletionFromUI) {
  InitializeBridge();
  StartSyncing();

  EXPECT_CALL(mock_foreign_session_updated_cb(), Run()).Times(0);
  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);

  bridge()->GetOpenTabsUIDelegate()->DeleteForeignSession(kLocalSessionTag);

  const SyncedSession* session = nullptr;
  EXPECT_TRUE(bridge()->GetOpenTabsUIDelegate()->GetLocalSession(&session));
  EXPECT_THAT(session, NotNull());
  EXPECT_THAT(GetData(SessionStore::GetHeaderStorageKey(kLocalSessionTag)),
              NotNull());
}

// Verifies that receiving an empty update list does not broadcast a foreign
// session change via the corresponding callback.
TEST_F(SessionSyncBridgeTest, ShouldNotBroadcastUpdatesIfEmpty) {
  InitializeBridge();
  StartSyncing();

  EXPECT_CALL(mock_foreign_session_updated_cb(), Run()).Times(0);

  // Mimic receiving an empty list of remote updates.
  sync_pb::ModelTypeState state;
  state.set_initial_sync_done(true);
  real_processor()->OnUpdateReceived(state, {});
}

TEST_F(SessionSyncBridgeTest, ShouldDoGarbageCollection) {
  // We construct two identical sessions, one modified recently, one modified
  // more than |kStaleSessionThreshold| ago (14 days ago).
  const base::Time stale_mtime =
      base::Time::Now() - base::TimeDelta::FromDays(15);
  const base::Time recent_mtime =
      base::Time::Now() - base::TimeDelta::FromDays(13);
  const std::string kStaleSessionTag = "stalesessiontag";
  const std::string kRecentSessionTag = "recentsessiontag";
  const int kWindowId = 2000001;
  const int kTabId = 2000002;
  const int kTabNodeId = 2003;

  InitializeBridge();
  StartSyncing();

  // Construct a remote update.
  sync_pb::ModelTypeState state;
  state.set_initial_sync_done(true);
  syncer::UpdateResponseDataList updates;
  // Two entities belong to a recent session.
  updates.push_back(SpecificsToUpdateResponse(
      CreateHeaderSpecificsWithOneTab(kStaleSessionTag, kWindowId, kTabId),
      stale_mtime));
  updates.push_back(SpecificsToUpdateResponse(
      CreateTabSpecifics(kStaleSessionTag, kWindowId, kTabId, kTabNodeId,
                         "http://baz.com/"),
      stale_mtime));
  updates.push_back(SpecificsToUpdateResponse(
      CreateHeaderSpecificsWithOneTab(kRecentSessionTag, kWindowId, kTabId),
      recent_mtime));
  updates.push_back(SpecificsToUpdateResponse(
      CreateTabSpecifics(kRecentSessionTag, kWindowId, kTabId, kTabNodeId,
                         "http://baz.com/"),
      recent_mtime));

  // During garbage collection, we expect |kStaleSessionTag| to be deleted.
  EXPECT_CALL(mock_processor(),
              Delete(SessionStore::GetHeaderStorageKey(kStaleSessionTag), _));
  EXPECT_CALL(
      mock_processor(),
      Delete(SessionStore::GetTabStorageKey(kStaleSessionTag, kTabNodeId), _));

  EXPECT_CALL(mock_foreign_session_updated_cb(), Run()).Times(AtLeast(1));
  real_processor()->OnUpdateReceived(state, std::move(updates));
}

}  // namespace
}  // namespace sync_sessions
