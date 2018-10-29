// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/sessions_sync_manager.h"

#include <stdint.h>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/sync_sessions/session_sync_test_helper.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/test_synced_window_delegates_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::DeviceInfo;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncDataLocal;
using syncer::SyncError;
using testing::ElementsAre;
using testing::Eq;
using testing::IsNull;

namespace sync_sessions {

namespace {

const char kCacheGuid[] = "cache_guid";
const char kFoo1[] = "http://foo1/";
const char kFoo2[] = "http://foo2/";
const char kBar1[] = "http://bar1/";
const char kBar2[] = "http://bar2/";
const char kBaz1[] = "http://baz1/";
const char kTag1[] = "tag1";
const char kTag2[] = "tag2";

std::vector<SessionID> SessionIDs(const std::vector<SessionID::id_type>& ids) {
  std::vector<SessionID> result;
  for (SessionID::id_type id : ids) {
    result.push_back(SessionID::FromSerializedValue(id));
  }
  return result;
}

std::string TabNodeIdToTag(const std::string& machine_tag, int tab_node_id) {
  return base::StringPrintf("%s %d", machine_tag.c_str(), tab_node_id);
}

size_t CountIfTagMatches(const SyncChangeList& changes,
                         const std::string& tag) {
  return std::count_if(
      changes.begin(), changes.end(), [&tag](const SyncChange& change) {
        return change.sync_data().GetSpecifics().session().session_tag() == tag;
      });
}

size_t CountIfTagMatches(const std::vector<const SyncedSession*>& sessions,
                         const std::string& tag) {
  return std::count_if(sessions.begin(), sessions.end(),
                       [&tag](const SyncedSession* session) {
                         return session->session_tag == tag;
                       });
}

testing::AssertionResult AllOfChangesAreType(
    const SyncChangeList& changes,
    const SyncChange::SyncChangeType type) {
  auto invalid_change = std::find_if(changes.begin(), changes.end(),
                                     [&type](const SyncChange& change) {
                                       return change.change_type() != type;
                                     });
  if (invalid_change != changes.end()) {
    return testing::AssertionFailure() << invalid_change->ToString()
                                       << " doesn't match "
                                       << SyncChange::ChangeTypeToString(type);
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult ChangeTypeMatches(
    const SyncChangeList& changes,
    const std::vector<SyncChange::SyncChangeType>& types) {
  auto types_iter = types.begin();
  if (changes.size() != types.size() ||
      std::any_of(changes.begin(), changes.end(),
                  [&types_iter](const SyncChange& change) {
                    SCOPED_TRACE(change.ToString());
                    return change.change_type() != *types_iter++;
                  })) {
    std::string type_string;
    std::for_each(types.begin(), types.end(),
                  [&type_string](const SyncChange::SyncChangeType& type) {
                    (type_string) += SyncChange::ChangeTypeToString(type) + " ";
                  });
    std::string change_string;
    std::for_each(changes.begin(), changes.end(),
                  [&change_string](const SyncChange& change) {
                    change_string += change.ToString();
                  });
    return testing::AssertionFailure()
           << "Change type mismatch: " << type_string << " vs "
           << change_string;
  }
  return testing::AssertionSuccess();
}

class TestSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  explicit TestSyncChangeProcessor(SyncChangeList* output) : output_(output) {}
  SyncError ProcessSyncChanges(const base::Location& from_here,
                               const SyncChangeList& change_list) override {
    if (error_.IsSet()) {
      SyncError error = error_;
      error_ = SyncError();
      return error;
    }

    if (output_)
      output_->insert(output_->end(), change_list.begin(), change_list.end());
    NotifyLocalChangeObservers();

    return SyncError();
  }

  SyncDataList GetAllSyncData(syncer::ModelType type) const override {
    return SyncDataList();
  }

  void AddLocalChangeObserver(syncer::LocalChangeObserver* observer) override {
    local_change_observers_.AddObserver(observer);
  }
  void RemoveLocalChangeObserver(
      syncer::LocalChangeObserver* observer) override {
    local_change_observers_.RemoveObserver(observer);
  }

  void NotifyLocalChangeObservers() {
    const SyncChange empty_change;
    for (syncer::LocalChangeObserver& observer : local_change_observers_)
      observer.OnLocalChange(nullptr, empty_change);
  }

  void FailProcessSyncChangesWith(const SyncError& error) { error_ = error; }

 private:
  SyncError error_;
  SyncChangeList* output_;
  base::ObserverList<syncer::LocalChangeObserver>::Unchecked
      local_change_observers_;
};

}  // namespace

class SessionsSyncManagerTest : public testing::Test {
 protected:
  const SessionID kWindowId1 = SessionID::FromSerializedValue(1);
  const SessionID kWindowId2 = SessionID::FromSerializedValue(2);
  const std::vector<SessionID> kTabIds1 = SessionIDs({5, 10, 13, 17});
  const std::vector<SessionID> kTabIds2 = SessionIDs({7, 15, 18, 20});

  SessionsSyncManagerTest()
      : local_device_info_(kCacheGuid,
                           "Wayne Gretzky's Hacking Box",
                           "Chromium 10k",
                           "Chrome 10k",
                           sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
                           "device_id"),
        session_sync_prefs_(&pref_service_) {}

  void SetUp() override {
    SessionSyncPrefs::RegisterProfilePrefs(pref_service_.registry());

    ON_CALL(mock_sync_sessions_client_, GetSessionSyncPrefs())
        .WillByDefault(testing::Return(&session_sync_prefs_));
    ON_CALL(mock_sync_sessions_client_, GetLocalDeviceInfo())
        .WillByDefault(testing::Return(&local_device_info_));
    ON_CALL(mock_sync_sessions_client_, GetSyncedWindowDelegatesGetter())
        .WillByDefault(testing::Return(&window_getter_));
    ON_CALL(mock_sync_sessions_client_, GetLocalSessionEventRouter())
        .WillByDefault(testing::Return(window_getter_.router()));

    manager_ =
        std::make_unique<SessionsSyncManager>(&mock_sync_sessions_client_);
  }

  void TearDown() override {
    test_processor_ = nullptr;
    helper()->Reset();
    manager_.reset();
  }

  const DeviceInfo* GetLocalDeviceInfo() { return &local_device_info_; }

  SessionsSyncManager* manager() { return manager_.get(); }
  SessionSyncTestHelper* helper() { return &helper_; }
  MockSyncSessionsClient* mock_sync_sessions_client() {
    return &mock_sync_sessions_client_;
  }

  void InitWithSyncDataTakeOutput(const SyncDataList& initial_data,
                                  SyncChangeList* output) {
    test_processor_ = new TestSyncChangeProcessor(output);
    syncer::SyncMergeResult result = manager_->MergeDataAndStartSyncing(
        syncer::SESSIONS, initial_data,
        std::unique_ptr<syncer::SyncChangeProcessor>(test_processor_),
        std::unique_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock()));
    EXPECT_FALSE(result.error().IsSet());
  }

  void InitWithNoSyncData() {
    InitWithSyncDataTakeOutput(SyncDataList(), nullptr);
  }

  void TriggerProcessSyncChangesError() {
    test_processor_->FailProcessSyncChangesWith(SyncError(
        FROM_HERE, SyncError::DATATYPE_ERROR, "Error", syncer::SESSIONS));
  }

  void VerifyLocalHeaderChange(const SyncChange& change,
                               int num_windows,
                               int num_tabs) {
    SCOPED_TRACE(change.ToString());
    SyncDataLocal data(change.sync_data());
    EXPECT_EQ(manager()->current_machine_tag(), data.GetTag());
    ASSERT_TRUE(data.GetSpecifics().session().has_header());
    EXPECT_FALSE(data.GetSpecifics().session().has_tab());
    EXPECT_TRUE(data.GetSpecifics().session().header().has_device_type());
    EXPECT_EQ(GetLocalDeviceInfo()->client_name(),
              data.GetSpecifics().session().header().client_name());
    EXPECT_EQ(num_windows,
              data.GetSpecifics().session().header().window_size());
    int tab_count = 0;
    for (auto& window : data.GetSpecifics().session().header().window()) {
      tab_count += window.tab_size();
    }
    EXPECT_EQ(num_tabs, tab_count);
  }

  void VerifyLocalTabChange(const SyncChange& change,
                            int num_navigations,
                            std::string final_url) {
    SCOPED_TRACE(change.ToString());
    SyncDataLocal data(change.sync_data());
    EXPECT_TRUE(base::StartsWith(data.GetTag(),
                                 manager()->current_machine_tag(),
                                 base::CompareCase::SENSITIVE));
    EXPECT_FALSE(data.GetSpecifics().session().has_header());
    ASSERT_TRUE(data.GetSpecifics().session().has_tab());
    ASSERT_EQ(num_navigations,
              data.GetSpecifics().session().tab().navigation_size());
    EXPECT_EQ(final_url, data.GetSpecifics()
                             .session()
                             .tab()
                             .navigation(num_navigations - 1)
                             .virtual_url());
  }

  SyncChangeList* FilterOutLocalHeaderChanges(SyncChangeList* list) {
    auto it = list->begin();
    bool found = false;
    while (it != list->end()) {
      if (it->sync_data().IsLocal() &&
          SyncDataLocal(it->sync_data()).GetTag() ==
              manager_->current_machine_tag()) {
        EXPECT_TRUE(SyncChange::ACTION_ADD == it->change_type() ||
                    SyncChange::ACTION_UPDATE == it->change_type());
        it = list->erase(it);
        found = true;
      } else {
        ++it;
      }
    }
    EXPECT_TRUE(found);
    return list;
  }

  SyncChange MakeRemoteChange(const sync_pb::SessionSpecifics& specifics,
                              SyncChange::SyncChangeType type) const {
    return SyncChange(FROM_HERE, type, CreateRemoteData(specifics));
  }

  void AddTabsToChangeList(const std::vector<sync_pb::SessionSpecifics>& batch,
                           SyncChange::SyncChangeType type,
                           SyncChangeList* change_list) const {
    for (const auto& specifics : batch) {
      change_list->push_back(
          SyncChange(FROM_HERE, type, CreateRemoteData(specifics)));
    }
  }

  void AddToSyncDataList(const sync_pb::SessionSpecifics& specifics,
                         SyncDataList* list,
                         base::Time mtime) const {
    list->push_back(CreateRemoteData(specifics, mtime));
  }

  void AddTabsToSyncDataList(const std::vector<sync_pb::SessionSpecifics>& tabs,
                             SyncDataList* list) const {
    for (size_t i = 0; i < tabs.size(); ++i) {
      AddToSyncDataList(tabs[i], list, base::Time::FromInternalValue(i + 1));
    }
  }

  SyncData CreateRemoteData(const sync_pb::SessionSpecifics& specifics,
                            base::Time mtime = base::Time()) const {
    sync_pb::EntitySpecifics entity;
    entity.mutable_session()->CopyFrom(specifics);
    return CreateRemoteData(entity, mtime);
  }

  SyncData CreateRemoteData(const sync_pb::EntitySpecifics& entity,
                            base::Time mtime = base::Time()) const {
    // The server ID is never relevant to these tests, so just use 1.
    return SyncData::CreateRemoteData(
        1, entity, mtime,
        SessionsSyncManager::TagHashFromSpecifics(entity.session()));
  }

  syncer::SyncDataList GetDataFromChanges(
      const syncer::SyncChangeList& changes) {
    syncer::SyncDataList data_list;
    for (auto& change : changes) {
      syncer::SyncDataLocal change_data(change.sync_data());
      bool found = false;
      for (auto&& data : data_list) {
        syncer::SyncDataLocal local_data(data);
        if (local_data.GetTag() == change_data.GetTag()) {
          data = change.sync_data();
          found = true;
          break;
        }
      }
      if (!found)
        data_list.push_back(change_data);
    }
    return data_list;
  }

  syncer::SyncDataList ConvertToRemote(const syncer::SyncDataList& in) {
    syncer::SyncDataList out;
    for (auto& data : in) {
      out.push_back(CreateRemoteData(data.GetSpecifics()));
    }
    return out;
  }

  void ResetWindows() { return window_getter_.ResetWindows(); }

  TestSyncedWindowDelegate* AddWindow(
      sync_pb::SessionWindow_BrowserType type =
          sync_pb::SessionWindow_BrowserType_TYPE_TABBED) {
    return window_getter_.AddWindow(type);
  }

  TestSyncedTabDelegate* AddTab(SessionID window_id, const std::string& url) {
    TestSyncedTabDelegate* tab = window_getter_.AddTab(window_id);
    tab->Navigate(url);
    return tab;
  }

 private:
  const syncer::DeviceInfo local_device_info_;
  TestingPrefServiceSimple pref_service_;
  SessionSyncPrefs session_sync_prefs_;
  testing::NiceMock<MockSyncSessionsClient> mock_sync_sessions_client_;
  std::unique_ptr<SessionsSyncManager> manager_;
  SessionSyncTestHelper helper_;
  TestSyncChangeProcessor* test_processor_ = nullptr;
  TestSyncedWindowDelegatesGetter window_getter_;
};

// Tests that the local session header objects is created properly in
// presence of no other session activity, once and only once.
TEST_F(SessionsSyncManagerTest, MergeLocalSessionNoTabs) {
  // Add a single window with no tabs.
  AddWindow();

  SyncChangeList out;
  InitWithSyncDataTakeOutput(SyncDataList(), &out);
  EXPECT_FALSE(manager()->current_machine_tag().empty());

  // Header creation + update.
  ASSERT_TRUE(ChangeTypeMatches(
      out, {SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));
  EXPECT_EQ(out.size(),
            CountIfTagMatches(out, manager()->current_machine_tag()));
  VerifyLocalHeaderChange(out[0], 0, 0);
  VerifyLocalHeaderChange(out[1], 0, 0);

  // Now take that header node and feed it in as input.
  SyncData d = CreateRemoteData(out[1].sync_data().GetSpecifics());
  SyncDataList in = {d};
  out.clear();
  manager()->StopSyncing(syncer::SESSIONS);
  InitWithSyncDataTakeOutput(in, &out);

  ASSERT_TRUE(ChangeTypeMatches(out, {SyncChange::ACTION_UPDATE}));
  EXPECT_TRUE(out[0].sync_data().GetSpecifics().session().has_header());
}

// Ensure that tabbed windows from a previous session are preserved if no
// windows are present on startup.
TEST_F(SessionsSyncManagerTest, PreserveTabbedDataNoWindows) {
  syncer::SyncDataList in;
  syncer::SyncChangeList out;

  // Set up one tab and start sync with it.
  TestSyncedTabDelegate* tab = AddTab(AddWindow()->GetSessionId(), kFoo1);
  tab->Navigate(kFoo2);
  InitWithSyncDataTakeOutput(in, &out);

  // There should be two entities, a header and a tab.
  in = GetDataFromChanges(out);
  out.clear();
  ASSERT_EQ(2U, in.size());

  // Resync, using the previous sync data, but with no windows open now.
  manager()->StopSyncing(syncer::SESSIONS);
  ResetWindows();
  InitWithSyncDataTakeOutput(ConvertToRemote(in), &out);

  // There should be one change to the rewritten header.
  ASSERT_TRUE(ChangeTypeMatches(out, {SyncChange::ACTION_UPDATE}));
  VerifyLocalHeaderChange(out[0], 1, 1);

  // SessionId should not be rewritten on restore.
  int restored_tab_id =
      out[0].sync_data().GetSpecifics().session().header().window(0).tab(0);
  EXPECT_EQ(tab->GetSessionId().id(), restored_tab_id);
  out.clear();

  // Now actually resurrect the native data, which will end up having different
  // native ids, but the tab has the same sync id as before.
  AddWindow()->OverrideTabAt(0, tab);
  tab->Navigate(kBar1);

  ASSERT_TRUE(ChangeTypeMatches(
      out, {SyncChange::ACTION_UPDATE, SyncChange::ACTION_UPDATE}));
  VerifyLocalTabChange(out[0], 3, kBar1);
  VerifyLocalHeaderChange(out[1], 1, 1);
}

// Ensure that tabbed windows from a previous session are preserved if only
// transient windows are present at startup.
TEST_F(SessionsSyncManagerTest, PreserveTabbedDataCustomTab) {
  syncer::SyncDataList in;
  syncer::SyncChangeList out;

  // Set up one tab and start sync with it.
  TestSyncedWindowDelegate* window = AddWindow();
  TestSyncedTabDelegate* tab = AddTab(window->GetSessionId(), kFoo1);
  tab->Navigate(kFoo2);
  InitWithSyncDataTakeOutput(in, &out);

  // There should be two entities, a header and a tab.
  in = GetDataFromChanges(out);
  out.clear();
  ASSERT_EQ(2U, in.size());

  // Resync, using the previous sync data, but with only a custom tab open.
  manager()->StopSyncing(syncer::SESSIONS);
  ResetWindows();
  window = AddWindow(sync_pb::SessionWindow_BrowserType_TYPE_CUSTOM_TAB);
  AddTab(window->GetSessionId(), kBar1);
  InitWithSyncDataTakeOutput(ConvertToRemote(in), &out);

  // The previous session should be preserved, together with the new custom tab.
  ASSERT_TRUE(ChangeTypeMatches(
      out, {SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));
  VerifyLocalTabChange(out[0], 1, kBar1);
  VerifyLocalHeaderChange(out[1], 2, 2);
}

// Tests MergeDataAndStartSyncing with sync data but no local data.
TEST_F(SessionsSyncManagerTest, MergeWithInitialForeignSession) {
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, kTabIds1, &tabs1));
  // Add a second window.
  helper()->AddWindowSpecifics(kWindowId2, kTabIds2, &meta);

  // Set up initial data.
  SyncDataList initial_data;
  initial_data.push_back(CreateRemoteData(meta));
  AddTabsToSyncDataList(tabs1, &initial_data);
  for (auto tab_id : kTabIds2) {
    sync_pb::EntitySpecifics entity;
    helper()->BuildTabSpecifics(kTag1, kWindowId1, tab_id,
                                entity.mutable_session());
    initial_data.push_back(CreateRemoteData(entity));
  }

  SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);
  EXPECT_TRUE(FilterOutLocalHeaderChanges(&output)->empty());

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID>> session_reference;
  session_reference.push_back(kTabIds1);
  session_reference.push_back(kTabIds2);
  helper()->VerifySyncedSession(kTag1, session_reference,
                                *(foreign_sessions[0]));
}

// Ensure model association associates the pre-existing tabs.
TEST_F(SessionsSyncManagerTest, MergeLocalSessionExistingTabs) {
  TestSyncedWindowDelegate* window = AddWindow();
  SessionID window_id = window->GetSessionId();
  TestSyncedTabDelegate* tab = AddTab(window_id, kFoo1);
  tab->Navigate(kBar1);  // Adds back entry.
  tab->Navigate(kBaz1);  // Adds back entry.
  TestSyncedTabDelegate* tab2 = AddTab(window_id, kFoo2);
  tab2->Navigate(kBar2);  // Adds back entry.

  SyncChangeList out;
  InitWithSyncDataTakeOutput(SyncDataList(), &out);
  // Header creation, add two tabs, header update.
  ASSERT_TRUE(
      ChangeTypeMatches(out,
                        {SyncChange::ACTION_ADD, SyncChange::ACTION_ADD,
                         SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));
  EXPECT_EQ(out.size(),
            CountIfTagMatches(out, manager()->current_machine_tag()));

  // Check that this machine's data is not included in the foreign windows.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_FALSE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));

  VerifyLocalHeaderChange(out[0], 0, 0);
  VerifyLocalTabChange(out[1], tab->GetEntryCount(), kBaz1);
  VerifyLocalTabChange(out[2], tab2->GetEntryCount(), kBar2);
  VerifyLocalHeaderChange(out[3], 1, 2);
}

// Ensure that the last known device name is reported.
TEST_F(SessionsSyncManagerTest, MergeLocalSessionName) {
  const std::string kModifiedDeviceName = "New Device Name";

  SyncChangeList out;
  InitWithSyncDataTakeOutput(SyncDataList(), &out);
  syncer::SyncDataList initial_data = GetDataFromChanges(out);
  // Local header expected.
  ASSERT_EQ(1U, initial_data.size());

  // Change local device name to |kModifiedDeviceName|.
  const DeviceInfo new_device_info(
      kCacheGuid, kModifiedDeviceName, "Chromium 10k", "Chrome 10k",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id");
  ON_CALL(*mock_sync_sessions_client(), GetLocalDeviceInfo())
      .WillByDefault(testing::Return(&new_device_info));

  // Restart the manager, now that the local device name has changed.
  manager()->StopSyncing(syncer::SESSIONS);
  out.clear();
  InitWithSyncDataTakeOutput(ConvertToRemote(initial_data), &out);

  EXPECT_EQ(kModifiedDeviceName, manager()->GetCurrentSessionNameForTest());
}

// This is a combination of MergeWithInitialForeignSession and
// MergeLocalSessionExistingTabs. We repeat some checks performed in each of
// those tests to ensure the common mixed scenario works.
TEST_F(SessionsSyncManagerTest, MergeWithLocalAndForeignTabs) {
  // Local.
  TestSyncedTabDelegate* tab = AddTab(AddWindow()->GetSessionId(), kFoo1);
  tab->Navigate(kFoo2);

  // Foreign.
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, kTabIds1, &tabs1));
  SyncDataList foreign_data;
  foreign_data.push_back(CreateRemoteData(meta));
  AddTabsToSyncDataList(tabs1, &foreign_data);

  SyncChangeList out;
  InitWithSyncDataTakeOutput(foreign_data, &out);

  // Should be one header add, 1 tab add, and one header update.
  ASSERT_TRUE(ChangeTypeMatches(out,
                                {SyncChange::ACTION_ADD, SyncChange::ACTION_ADD,
                                 SyncChange::ACTION_UPDATE}));
  EXPECT_EQ(out.size(),
            CountIfTagMatches(out, manager()->current_machine_tag()));

  // Verify local data.
  VerifyLocalHeaderChange(out[0], 0, 0);
  VerifyLocalTabChange(out[1], tab->GetEntryCount(), kFoo2);
  VerifyLocalHeaderChange(out[2], 1, 1);

  // Verify foreign data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  std::vector<std::vector<SessionID>> session_reference;
  session_reference.push_back(kTabIds1);
  helper()->VerifySyncedSession(kTag1, session_reference,
                                *(foreign_sessions[0]));
  // There should be one and only one foreign session. If VerifySyncedSession
  // was successful above this EXPECT call ensures the local session didn't
  // get mistakenly added to foreign tracking (Similar to ExistingTabs test).
  EXPECT_EQ(1U, foreign_sessions.size());
}

// Tests the common scenario.  Merge with both local and foreign session data
// followed by updates flowing from sync and local.
TEST_F(SessionsSyncManagerTest, UpdatesAfterMixedMerge) {
  // Add local and foreign data.
  TestSyncedTabDelegate* tab = AddTab(AddWindow()->GetSessionId(), kFoo1);
  tab->Navigate(kFoo2);
  AddTab(AddWindow()->GetSessionId(), kBar1);

  SyncDataList foreign_data1;
  std::vector<std::vector<SessionID>> meta1_reference;
  sync_pb::SessionSpecifics meta1;

  meta1_reference.push_back(kTabIds1);
  std::vector<sync_pb::SessionSpecifics> tabs1;
  meta1 = helper()->BuildForeignSession(kTag1, kTabIds1, &tabs1);
  foreign_data1.push_back(CreateRemoteData(meta1));
  AddTabsToSyncDataList(tabs1, &foreign_data1);

  SyncChangeList out;
  InitWithSyncDataTakeOutput(foreign_data1, &out);

  // 1 header add, two tab adds, one header update.
  ASSERT_TRUE(
      ChangeTypeMatches(out,
                        {SyncChange::ACTION_ADD, SyncChange::ACTION_ADD,
                         SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));
  EXPECT_EQ(out.size(),
            CountIfTagMatches(out, manager()->current_machine_tag()));
  VerifyLocalHeaderChange(out[3], 2, 2);

  // Add a second window to the foreign session.
  meta1_reference.push_back(kTabIds2);
  helper()->AddWindowSpecifics(kWindowId2, kTabIds2, &meta1);
  std::vector<sync_pb::SessionSpecifics> tabs2;
  tabs2.resize(kTabIds2.size());
  for (size_t i = 0; i < kTabIds2.size(); ++i) {
    helper()->BuildTabSpecifics(kTag1, kWindowId2, kTabIds2[i], &tabs2[i]);
  }

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(meta1, SyncChange::ACTION_UPDATE));
  AddTabsToChangeList(tabs2, SyncChange::ACTION_ADD, &changes);
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  changes.clear();

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  ASSERT_EQ(4U, foreign_sessions[0]
                    ->windows.find(kWindowId1)
                    ->second->wrapped_window.tabs.size());
  ASSERT_EQ(4U, foreign_sessions[0]
                    ->windows.find(kWindowId2)
                    ->second->wrapped_window.tabs.size());
  helper()->VerifySyncedSession(kTag1, meta1_reference, *(foreign_sessions[0]));

  // Add a new foreign session.
  const std::vector<SessionID> tag2_tab_list = SessionIDs({107, 115});
  std::vector<sync_pb::SessionSpecifics> tag2_tabs;
  sync_pb::SessionSpecifics meta2(
      helper()->BuildForeignSession(kTag2, tag2_tab_list, &tag2_tabs));
  changes.push_back(MakeRemoteChange(meta2, SyncChange::ACTION_ADD));
  AddTabsToChangeList(tag2_tabs, SyncChange::ACTION_ADD, &changes);

  manager()->ProcessSyncChanges(FROM_HERE, changes);
  changes.clear();

  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  std::vector<std::vector<SessionID>> meta2_reference;
  meta2_reference.push_back(tag2_tab_list);
  ASSERT_EQ(2U, foreign_sessions.size());
  ASSERT_EQ(2U, foreign_sessions[1]
                    ->windows.find(kWindowId1)
                    ->second->wrapped_window.tabs.size());
  helper()->VerifySyncedSession(kTag2, meta2_reference, *(foreign_sessions[1]));
  foreign_sessions.clear();

  // Remove a tab from a window.
  meta1_reference[0].pop_back();
  sync_pb::SessionWindow* win = meta1.mutable_header()->mutable_window(0);
  win->clear_tab();
  for (auto iter = kTabIds1.begin(); iter + 1 != kTabIds1.end(); ++iter) {
    win->add_tab(iter->id());
  }
  SyncChangeList removal;
  removal.push_back(MakeRemoteChange(meta1, SyncChange::ACTION_UPDATE));
  AddTabsToChangeList(tabs1, SyncChange::ACTION_UPDATE, &removal);
  manager()->ProcessSyncChanges(FROM_HERE, removal);

  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(2U, foreign_sessions.size());
  ASSERT_EQ(3U, foreign_sessions[0]
                    ->windows.find(kWindowId1)
                    ->second->wrapped_window.tabs.size());
  helper()->VerifySyncedSession(kTag1, meta1_reference, *(foreign_sessions[0]));
}

// Tests that this SyncSessionManager knows how to delete foreign sessions
// if it wants to.
TEST_F(SessionsSyncManagerTest, DeleteForeignSession) {
  InitWithNoSyncData();
  SyncChangeList changes;

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_FALSE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  manager()->DeleteForeignSessionInternal(kTag1, &changes);
  ASSERT_FALSE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  EXPECT_TRUE(changes.empty());

  // Fill an instance of session specifics with a foreign session's data.
  std::vector<sync_pb::SessionSpecifics> tabs;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, kTabIds1, &tabs));

  // Update associator with the session's meta node, window, and tabs.
  UpdateTrackerWithSpecifics(meta, base::Time(), &manager()->session_tracker_);
  for (auto iter = tabs.begin(); iter != tabs.end(); ++iter) {
    UpdateTrackerWithSpecifics(*iter, base::Time(),
                               &manager()->session_tracker_);
  }
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());

  // Now delete the foreign session.
  manager()->DeleteForeignSessionInternal(kTag1, &changes);
  EXPECT_FALSE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));

  EXPECT_EQ(5U, changes.size());
  ASSERT_TRUE(AllOfChangesAreType(changes, SyncChange::ACTION_DELETE));
  std::set<std::string> expected_tags(&kTag1, &kTag1 + 1);
  for (int i = 0; i < 5; ++i)
    expected_tags.insert(TabNodeIdToTag(kTag1, i));

  for (int i = 0; i < 5; ++i) {
    SCOPED_TRACE(changes[i].ToString());
    EXPECT_TRUE(changes[i].IsValid());
    EXPECT_TRUE(changes[i].sync_data().IsValid());
    EXPECT_EQ(1U, expected_tags.erase(
                      SyncDataLocal(changes[i].sync_data()).GetTag()));
  }
}

// Write a foreign session to a node, with the tabs arriving first, and then
// retrieve it.
TEST_F(SessionsSyncManagerTest, WriteForeignSessionToNodeTabsFirst) {
  InitWithNoSyncData();

  // Fill an instance of session specifics with a foreign session's data.
  std::string tag = "tag1";
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(tag, kTabIds1, &tabs1));

  SyncChangeList adds;
  // Add tabs for first window, then the meta node.
  AddTabsToChangeList(tabs1, SyncChange::ACTION_ADD, &adds);
  adds.push_back(MakeRemoteChange(meta, SyncChange::ACTION_ADD));
  manager()->ProcessSyncChanges(FROM_HERE, adds);

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID>> session_reference;
  session_reference.push_back(kTabIds1);
  helper()->VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

// Write a foreign session to a node with some tabs that never arrive.
TEST_F(SessionsSyncManagerTest, WriteForeignSessionToNodeMissingTabs) {
  InitWithNoSyncData();

  // Fill an instance of session specifics with a foreign session's data.
  std::string tag = "tag1";
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(tag, kTabIds1, &tabs1));
  // Add a second window, but this time only create two tab nodes, despite the
  // window expecting four tabs.
  helper()->AddWindowSpecifics(kWindowId2, kTabIds2, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs2;
  tabs2.resize(2);
  for (size_t i = 0; i < 2; ++i) {
    helper()->BuildTabSpecifics(tag, kWindowId1, kTabIds2[i], &tabs2[i]);
  }

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_ADD));
  AddTabsToChangeList(tabs1, SyncChange::ACTION_ADD, &changes);
  AddTabsToChangeList(tabs2, SyncChange::ACTION_ADD, &changes);
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  changes.clear();

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  ASSERT_EQ(2U, foreign_sessions[0]->windows.size());
  ASSERT_EQ(4U, foreign_sessions[0]
                    ->windows.find(kWindowId1)
                    ->second->wrapped_window.tabs.size());
  ASSERT_EQ(4U, foreign_sessions[0]
                    ->windows.find(kWindowId2)
                    ->second->wrapped_window.tabs.size());

  // Close the second window.
  meta.mutable_header()->clear_window();
  helper()->AddWindowSpecifics(kWindowId1, kTabIds1, &meta);
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_UPDATE));
  // Update associator with the session's meta node containing one window.
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  // Check that the foreign session was associated and retrieve the data.
  foreign_sessions.clear();
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  ASSERT_EQ(1U, foreign_sessions[0]->windows.size());
  std::vector<std::vector<SessionID>> session_reference;
  session_reference.push_back(kTabIds1);
  helper()->VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

// Tests that the SessionsSyncManager can handle a remote client deleting
// sync nodes that belong to this local session.
TEST_F(SessionsSyncManagerTest, ProcessRemoteDeleteOfLocalSession) {
  SessionID window_id = AddWindow()->GetSessionId();
  SyncChangeList out;
  InitWithSyncDataTakeOutput(SyncDataList(), &out);
  ASSERT_EQ(2U, out.size());

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(
      out[1].sync_data().GetSpecifics().session(), SyncChange::ACTION_DELETE));
  out.clear();
  manager()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(manager()->local_tab_pool_out_of_sync_);
  EXPECT_TRUE(out.empty());  // ChangeProcessor shouldn't see any activity.

  // This should trigger repair of the TabNodePool.
  AddTab(window_id, kFoo1);
  EXPECT_FALSE(manager()->local_tab_pool_out_of_sync_);

  // Rebuilding associations will trigger an initial header add and update,
  // coupled with the tab creation and the header update to reflect the new tab.
  // In total, that means four changes.
  ASSERT_TRUE(
      ChangeTypeMatches(out,
                        {SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE,
                         SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));

  // Verify the actual content.
  VerifyLocalTabChange(out[2], 1, kFoo1);
  VerifyLocalHeaderChange(out[3], 1, 1);

  // Verify TabLinks.
  int tab_node_id = out[2].sync_data().GetSpecifics().session().tab_node_id();
  int tab_id = out[2].sync_data().GetSpecifics().session().tab().tab_id();
  EXPECT_EQ(tab_id, manager()
                        ->session_tracker_
                        .LookupTabIdFromTabNodeId(
                            manager()->current_machine_tag(), tab_node_id)
                        .id());
}

// Test that receiving a session delete from sync removes the session
// from tracking.
TEST_F(SessionsSyncManagerTest, ProcessForeignDelete) {
  InitWithNoSyncData();
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession("tag1", kTabIds1, &tabs1));

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_ADD));
  AddTabsToChangeList(tabs1, SyncChange::ACTION_ADD, &changes);
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());

  changes.clear();
  foreign_sessions.clear();
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_FALSE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
}

TEST_F(SessionsSyncManagerTest, ProcessForeignDeleteTabs) {
  SyncDataList foreign_data;
  base::Time stale_mtime = base::Time::Now() - base::TimeDelta::FromDays(15);
  std::string session_tag = "tag1";

  // 1 will not have ownership changed.
  // 2 will not be updated, but header will stop owning.
  // 3 will be deleted before header stops owning.
  // 4 will be deleted after header stops owning.
  // 5 will be deleted before header update, but header will still try to own.
  // 6 will be deleted after header update, but header will still try to own.
  // 7 starts orphaned and then deleted before header update.
  // 8 starts orphaned and then deleted after header update.
  const std::vector<SessionID> tab_list = SessionIDs({1, 2, 3, 4, 5, 6});
  std::vector<sync_pb::SessionSpecifics> tabs;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(session_tag, tab_list, &tabs));
  AddToSyncDataList(meta, &foreign_data, stale_mtime);
  AddTabsToSyncDataList(tabs, &foreign_data);
  sync_pb::SessionSpecifics orphan6;
  helper()->BuildTabSpecifics(session_tag, kWindowId1,
                              SessionID::FromSerializedValue(6), &orphan6);
  AddToSyncDataList(orphan6, &foreign_data, stale_mtime);
  sync_pb::SessionSpecifics orphan7;
  helper()->BuildTabSpecifics(session_tag, kWindowId1,
                              SessionID::FromSerializedValue(7), &orphan7);
  AddToSyncDataList(orphan7, &foreign_data, stale_mtime);

  AddWindow();
  SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  const std::vector<SessionID> update_list = SessionIDs({1, 5, 6});
  sync_pb::SessionWindow* window = meta.mutable_header()->mutable_window(0);
  window->clear_tab();
  for (SessionID i : update_list) {
    window->add_tab(i.id());
  }

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(tabs[2], SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(tabs[4], SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(orphan6, SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_UPDATE));
  changes.push_back(MakeRemoteChange(tabs[3], SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(tabs[5], SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(orphan7, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID>> session_reference;
  session_reference.push_back(update_list);
  helper()->VerifySyncedSession(session_tag, session_reference,
                                *(foreign_sessions[0]));

  // Everything except for session, tab0, and tab1 will have no node_id, and
  // should get skipped by garbage collection.
  manager()->DoGarbageCollection();
  ASSERT_EQ(3U, output.size());
}

TEST_F(SessionsSyncManagerTest, ProcessForeignDeleteTabsWithShadowing) {
  SyncDataList foreign_data;
  base::Time stale_mtime = base::Time::Now() - base::TimeDelta::FromDays(16);
  std::string session_tag = "tag1";

  // Add several tabs that shadow eachother, in that they share tab_ids. They
  // will, thanks to the helper, have unique tab_node_ids.
  sync_pb::SessionSpecifics tab1A;
  helper()->BuildTabSpecifics(session_tag, kWindowId1, kTabIds1[0], &tab1A);
  AddToSyncDataList(tab1A, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(1));

  sync_pb::SessionSpecifics tab1B;
  helper()->BuildTabSpecifics(session_tag, kWindowId1, kTabIds1[0], &tab1B);
  AddToSyncDataList(tab1B, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(2));

  sync_pb::SessionSpecifics tab1C;
  helper()->BuildTabSpecifics(session_tag, kWindowId1, kTabIds1[0], &tab1C);
  AddToSyncDataList(tab1C, &foreign_data, stale_mtime);

  sync_pb::SessionSpecifics tab2A;
  helper()->BuildTabSpecifics(session_tag, kWindowId1, kTabIds1[1], &tab2A);
  AddToSyncDataList(tab2A, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(1));

  sync_pb::SessionSpecifics tab2B;
  helper()->BuildTabSpecifics(session_tag, kWindowId1, kTabIds1[1], &tab2B);
  AddToSyncDataList(tab2B, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(2));

  sync_pb::SessionSpecifics tab2C;
  helper()->BuildTabSpecifics(session_tag, kWindowId1, kTabIds1[1], &tab2C);
  AddToSyncDataList(tab2C, &foreign_data, stale_mtime);

  AddWindow();
  SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  // Verify that cleanup post-merge cleanup correctly removes all tabs objects.
  ASSERT_THAT(
      manager()->session_tracker_.LookupSessionTab(session_tag, kTabIds1[0]),
      IsNull());
  ASSERT_THAT(
      manager()->session_tracker_.LookupSessionTab(session_tag, kTabIds1[1]),
      IsNull());

  EXPECT_THAT(manager()->session_tracker_.LookupTabNodeIds(session_tag),
              ElementsAre(tab1A.tab_node_id(), tab1B.tab_node_id(),
                          tab1C.tab_node_id(), tab2A.tab_node_id(),
                          tab2B.tab_node_id(), tab2C.tab_node_id()));

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(tab1A, SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(tab1B, SyncChange::ACTION_DELETE));
  changes.push_back(MakeRemoteChange(tab2C, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_THAT(manager()->session_tracker_.LookupTabNodeIds(session_tag),
              ElementsAre(tab1C.tab_node_id(), tab2A.tab_node_id(),
                          tab2B.tab_node_id()));

  manager()->DoGarbageCollection();
  ASSERT_EQ(3U, output.size());
}

TEST_F(SessionsSyncManagerTest, ProcessForeignDeleteTabsWithReusedNodeIds) {
  SyncDataList foreign_data;
  base::Time stale_mtime = base::Time::Now() - base::TimeDelta::FromDays(16);
  std::string session_tag = "tag1";
  int tab_node_id_shared = 13;
  int tab_node_id_unique = 14;

  sync_pb::SessionSpecifics tab1A;
  helper()->BuildTabSpecifics(session_tag, kWindowId1, kTabIds1[1],
                              tab_node_id_shared, &tab1A);
  AddToSyncDataList(tab1A, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(1));

  sync_pb::SessionSpecifics tab1B;
  helper()->BuildTabSpecifics(session_tag, kWindowId1, kTabIds1[1],
                              tab_node_id_unique, &tab1B);
  AddToSyncDataList(tab1B, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(2));

  sync_pb::SessionSpecifics tab2A;
  helper()->BuildTabSpecifics(session_tag, kWindowId1, kTabIds1[2],
                              tab_node_id_shared, &tab2A);
  AddToSyncDataList(tab2A, &foreign_data,
                    stale_mtime + base::TimeDelta::FromMinutes(1));

  AddWindow();
  SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  EXPECT_THAT(manager()->session_tracker_.LookupTabNodeIds(session_tag),
              ElementsAre(tab_node_id_shared, tab_node_id_unique));

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(tab1A, SyncChange::ACTION_DELETE));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_THAT(manager()->session_tracker_.LookupTabNodeIds(session_tag),
              ElementsAre(tab_node_id_unique));

  manager()->DoGarbageCollection();
  EXPECT_EQ(1U, output.size());
}

TEST_F(SessionsSyncManagerTest, AssociationReusesNodes) {
  SyncChangeList changes;
  TestSyncedWindowDelegate* window = AddWindow();
  TestSyncedTabDelegate* tab = AddTab(window->GetSessionId(), kFoo1);
  InitWithSyncDataTakeOutput(SyncDataList(), &changes);
  ASSERT_TRUE(ChangeTypeMatches(changes,
                                {SyncChange::ACTION_ADD, SyncChange::ACTION_ADD,
                                 SyncChange::ACTION_UPDATE}));
  ASSERT_TRUE(changes[1].sync_data().GetSpecifics().session().has_tab());
  int tab_node_id =
      changes[1].sync_data().GetSpecifics().session().tab_node_id();

  // Pass back the previous tab and header nodes at association, along with a
  // second tab node (with rewritten tab IDs).
  SyncDataList in;
  in.push_back(
      CreateRemoteData(changes[2].sync_data().GetSpecifics()));  // Header node.
  sync_pb::SessionSpecifics new_tab(
      changes[1].sync_data().GetSpecifics().session());
  new_tab.mutable_tab()->set_tab_id(new_tab.tab().tab_id() + 1);
  new_tab.set_tab_node_id(tab_node_id + 1);
  in.push_back(CreateRemoteData(
      changes[1].sync_data().GetSpecifics()));  // Old tab node.
  in.push_back(CreateRemoteData(new_tab));      // New tab node.
  changes.clear();

  // Reassociate (with the same single tab/window open).
  manager()->StopSyncing(syncer::SESSIONS);
  InitWithSyncDataTakeOutput(in, &changes);

  // No tab entities should be deleted. The original (lower) tab node id should
  // be reused for association.
  FilterOutLocalHeaderChanges(&changes);
  ASSERT_TRUE(ChangeTypeMatches(changes, {SyncChange::ACTION_UPDATE}));
  VerifyLocalTabChange(changes[0], 1, kFoo1);
  EXPECT_EQ(tab_node_id,
            changes[0].sync_data().GetSpecifics().session().tab_node_id());
  changes.clear();

  // Update the original tab. Ensure the same tab node is updated.
  tab->Navigate(kFoo2);
  FilterOutLocalHeaderChanges(&changes);
  ASSERT_TRUE(ChangeTypeMatches(changes, {SyncChange::ACTION_UPDATE}));
  VerifyLocalTabChange(changes[0], 2, kFoo2);
  EXPECT_EQ(tab_node_id,
            changes[0].sync_data().GetSpecifics().session().tab_node_id());
  changes.clear();

  // Add a new tab. It should reuse the second tab node.
  AddTab(window->GetSessionId(), kBar1);
  FilterOutLocalHeaderChanges(&changes);
  ASSERT_TRUE(ChangeTypeMatches(changes, {SyncChange::ACTION_UPDATE}));
  VerifyLocalTabChange(changes[0], 1, kBar1);
  EXPECT_EQ(tab_node_id + 1,
            changes[0].sync_data().GetSpecifics().session().tab_node_id());
}

// Ensure that the merge process deletes a tab node without a tab id.
TEST_F(SessionsSyncManagerTest, MergeDeletesTabMissingTabId) {
  SyncChangeList changes;
  InitWithNoSyncData();

  std::string local_tag = manager()->current_machine_tag();
  int tab_node_id = 0;
  sync_pb::SessionSpecifics specifics;
  specifics.set_session_tag(local_tag);
  specifics.set_tab_node_id(tab_node_id);
  manager()->StopSyncing(syncer::SESSIONS);
  InitWithSyncDataTakeOutput({CreateRemoteData(specifics)}, &changes);
  EXPECT_EQ(1U, FilterOutLocalHeaderChanges(&changes)->size());
  EXPECT_EQ(SyncChange::ACTION_DELETE, changes[0].change_type());
  EXPECT_EQ(TabNodeIdToTag(local_tag, tab_node_id),
            SyncDataLocal(changes[0].sync_data()).GetTag());
}

// Verifies that we drop both headers and tabs during merge if their stored tag
// hash doesn't match a computer tag hash. This mitigates potential failures
// while cleaning up bad foreign data, see https://crbug.com/604657.
TEST_F(SessionsSyncManagerTest, MergeDeletesBadHash) {
  SyncDataList foreign_data;
  std::vector<SessionID> empty_ids;
  std::vector<sync_pb::SessionSpecifics> empty_tabs;
  sync_pb::EntitySpecifics entity;

  const std::string good_header_tag = "good_header_tag";
  sync_pb::SessionSpecifics good_header(
      helper()->BuildForeignSession(good_header_tag, empty_ids, &empty_tabs));
  foreign_data.push_back(CreateRemoteData(good_header));

  const std::string bad_header_tag = "bad_header_tag";
  sync_pb::SessionSpecifics bad_header(
      helper()->BuildForeignSession(bad_header_tag, empty_ids, &empty_tabs));
  entity.mutable_session()->CopyFrom(bad_header);
  foreign_data.push_back(SyncData::CreateRemoteData(1, entity, base::Time(),
                                                    "bad_header_tag_hash"));

  const std::string good_tag_tab = "good_tag_tab";
  sync_pb::SessionSpecifics good_tab;
  helper()->BuildTabSpecifics(good_tag_tab, kWindowId1, kTabIds1[0], &good_tab);
  foreign_data.push_back(CreateRemoteData(good_tab));

  const std::string bad_tab_tag = "bad_tab_tag";
  sync_pb::SessionSpecifics bad_tab;
  helper()->BuildTabSpecifics(bad_tab_tag, kWindowId1, kTabIds1[1], &bad_tab);
  entity.mutable_session()->CopyFrom(bad_tab);
  foreign_data.push_back(
      SyncData::CreateRemoteData(1, entity, base::Time(), "bad_tab_tag_hash"));

  SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, FilterOutLocalHeaderChanges(&output)->size());
  ASSERT_TRUE(AllOfChangesAreType(output, SyncChange::ACTION_DELETE));
  EXPECT_EQ(1U, CountIfTagMatches(output, bad_header_tag));
  EXPECT_EQ(1U, CountIfTagMatches(output, bad_tab_tag));

  const std::vector<const SyncedSession*> sessions =
      manager()->session_tracker_.LookupAllForeignSessions(
          SyncedSessionTracker::RAW);
  ASSERT_EQ(2U, sessions.size());
  EXPECT_EQ(1U, CountIfTagMatches(sessions, good_header_tag));
  EXPECT_EQ(1U, CountIfTagMatches(sessions, good_tag_tab));
}

// Test that things work if a tab is initially ignored.
TEST_F(SessionsSyncManagerTest, AssociateWindowsDontReloadTabs) {
  SyncChangeList out;
  // Go to a URL that is ignored by session syncing.
  TestSyncedTabDelegate* tab =
      AddTab(AddWindow()->GetSessionId(), "chrome://preferences/");
  InitWithSyncDataTakeOutput(SyncDataList(), &out);
  ASSERT_TRUE(ChangeTypeMatches(
      out, {SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));
  VerifyLocalHeaderChange(out[1], 0, 0);
  out.clear();

  // Go to a sync-interesting URL.
  tab->Navigate(kFoo1);

  // The tab should be created, coupled with a header update.
  ASSERT_TRUE(ChangeTypeMatches(
      out, {SyncChange::ACTION_ADD, SyncChange::ACTION_UPDATE}));
  VerifyLocalTabChange(out[0], 2, kFoo1);
  VerifyLocalHeaderChange(out[1], 1, 1);
}

// Tests that the SyncSessionManager responds to local tab events properly.
TEST_F(SessionsSyncManagerTest, OnLocalTabModified) {
  SyncChangeList out;
  // Init with no local data, relies on MergeLocalSessionNoTabs.
  TestSyncedWindowDelegate* window = AddWindow();
  SessionID window_id = window->GetSessionId();
  InitWithSyncDataTakeOutput(SyncDataList(), &out);
  ASSERT_FALSE(manager()->current_machine_tag().empty());
  ASSERT_EQ(2U, out.size());

  // Copy the original header.
  sync_pb::EntitySpecifics header(out[0].sync_data().GetSpecifics());
  out.clear();

  AddTab(window_id, kFoo1)->Navigate(kFoo2);
  AddTab(window_id, kBar1)->Navigate(kBar2);
  std::vector<std::string> urls = {kFoo1, kFoo2, kBar1, kBar2};

  // Change type breakdown:
  // 1 tab add + 2 header updates.
  const size_t kChangesPerTabCreation = 3;
  // 1 tab update + 1 header update.
  const size_t kChangesPerTabNav = 2;
  const size_t kChangesPerTab = kChangesPerTabNav + kChangesPerTabCreation;
  const size_t kNumTabs = 2;
  const size_t kTotalUpdates = kChangesPerTab * kNumTabs;

  std::vector<SyncChange::SyncChangeType> types = {
      // Tab 1
      SyncChange::ACTION_UPDATE, SyncChange::ACTION_ADD,
      SyncChange::ACTION_UPDATE, SyncChange::ACTION_UPDATE,
      SyncChange::ACTION_UPDATE,
      // Tab 2
      SyncChange::ACTION_UPDATE, SyncChange::ACTION_ADD,
      SyncChange::ACTION_UPDATE, SyncChange::ACTION_UPDATE,
      SyncChange::ACTION_UPDATE};
  ASSERT_EQ(kTotalUpdates, types.size());

  // Verify the tab node creations and updates to ensure the SyncProcessor sees
  // the right operations. Do this by inspecting the set of changes for each
  // tab separately by iterating through the tabs.
  ASSERT_TRUE(ChangeTypeMatches(out, types));
  for (size_t i = 0; i < kNumTabs; ++i) {
    int index = kChangesPerTab * i;
    int nav_per_tab_count = 0;
    {
      SCOPED_TRACE(index);
      // The initial tab parent event triggers a header update (which is in
      // effect a no-op).
      VerifyLocalHeaderChange(out[index++], (i == 0 ? 0 : 1), i);
    }
    {
      SCOPED_TRACE(index);
      nav_per_tab_count++;
      // Tab update after initial creation..
      VerifyLocalTabChange(out[index++], nav_per_tab_count,
                           urls[i * kChangesPerTabNav + nav_per_tab_count - 1]);
    }
    {
      SCOPED_TRACE(index);
      // The associate windows after the tab creation.
      VerifyLocalHeaderChange(out[index++], 1, i + 1);
    }
    {
      SCOPED_TRACE(index);
      nav_per_tab_count++;
      // Tab navigation.
      VerifyLocalTabChange(out[index++], nav_per_tab_count,
                           urls[i * kChangesPerTabNav + nav_per_tab_count - 1]);
    }
    {
      SCOPED_TRACE(index);
      // The associate windows after the tab navigation.
      VerifyLocalHeaderChange(out[index++], 1, i + 1);
    }
  }
}

TEST_F(SessionsSyncManagerTest, ForeignSessionModifiedTime) {
  SyncDataList foreign_data;
  base::Time newest_time = base::Time::Now() - base::TimeDelta::FromDays(1);
  base::Time middle_time = base::Time::Now() - base::TimeDelta::FromDays(2);
  base::Time oldest_time = base::Time::Now() - base::TimeDelta::FromDays(3);

  {
    std::string session_tag = "tag1";
    std::vector<sync_pb::SessionSpecifics> tabs;
    sync_pb::SessionSpecifics meta(
        helper()->BuildForeignSession(session_tag, SessionIDs({1, 2}), &tabs));
    AddToSyncDataList(tabs[0], &foreign_data, newest_time);
    AddToSyncDataList(meta, &foreign_data, middle_time);
    AddToSyncDataList(tabs[1], &foreign_data, oldest_time);
  }

  {
    std::string session_tag = "tag2";
    std::vector<sync_pb::SessionSpecifics> tabs;
    sync_pb::SessionSpecifics meta(
        helper()->BuildForeignSession(session_tag, SessionIDs({3, 4}), &tabs));
    AddToSyncDataList(tabs[0], &foreign_data, middle_time);
    AddToSyncDataList(meta, &foreign_data, newest_time);
    AddToSyncDataList(tabs[1], &foreign_data, oldest_time);
  }

  {
    std::string session_tag = "tag3";
    std::vector<sync_pb::SessionSpecifics> tabs;
    sync_pb::SessionSpecifics meta(
        helper()->BuildForeignSession(session_tag, SessionIDs({5, 6}), &tabs));
    AddToSyncDataList(tabs[0], &foreign_data, oldest_time);
    AddToSyncDataList(meta, &foreign_data, middle_time);
    AddToSyncDataList(tabs[1], &foreign_data, newest_time);
  }

  SyncChangeList output;
  AddWindow();
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(3U, foreign_sessions.size());
  EXPECT_EQ(newest_time, foreign_sessions[0]->modified_time);
  EXPECT_EQ(newest_time, foreign_sessions[1]->modified_time);
  EXPECT_EQ(newest_time, foreign_sessions[2]->modified_time);
}

// Test garbage collection of stale foreign sessions.
TEST_F(SessionsSyncManagerTest, DoGarbageCollection) {
  // Fill two instances of session specifics with a foreign session's data.
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, kTabIds1, &tabs1));
  std::vector<sync_pb::SessionSpecifics> tabs2;
  sync_pb::SessionSpecifics meta2(
      helper()->BuildForeignSession(kTag2, kTabIds2, &tabs2));
  // Set the modification time for tag1 to be 21 days ago, tag2 to 5 days ago.
  base::Time tag1_time = base::Time::Now() - base::TimeDelta::FromDays(21);
  base::Time tag2_time = base::Time::Now() - base::TimeDelta::FromDays(5);

  SyncDataList foreign_data;
  foreign_data.push_back(CreateRemoteData(meta, tag1_time));
  foreign_data.push_back(CreateRemoteData(meta2, tag2_time));
  AddTabsToSyncDataList(tabs1, &foreign_data);
  AddTabsToSyncDataList(tabs2, &foreign_data);

  AddWindow();
  SyncChangeList output;
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(2U, foreign_sessions.size());
  foreign_sessions.clear();

  // Now garbage collect and verify the non-stale session is still there.
  manager()->DoGarbageCollection();
  ASSERT_EQ(5U, output.size());
  ASSERT_TRUE(AllOfChangesAreType(output, SyncChange::ACTION_DELETE));
  EXPECT_EQ(kTag1, SyncDataLocal(output[0].sync_data()).GetTag());
  for (int i = 1; i < 5; ++i) {
    EXPECT_EQ(TabNodeIdToTag(kTag1, i),
              SyncDataLocal(output[i].sync_data()).GetTag());
  }

  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID>> session_reference;
  session_reference.push_back(kTabIds2);
  helper()->VerifySyncedSession(kTag2, session_reference,
                                *(foreign_sessions[0]));
}

TEST_F(SessionsSyncManagerTest, DoGarbageCollectionOrphans) {
  SyncDataList foreign_data;
  base::Time stale_mtime = base::Time::Now() - base::TimeDelta::FromDays(15);

  {
    // A stale session with empty header
    std::string session_tag = "tag1";
    std::vector<sync_pb::SessionSpecifics> tabs;
    sync_pb::SessionSpecifics meta(
        helper()->BuildForeignSession(session_tag, {}, &tabs));
    AddToSyncDataList(meta, &foreign_data, stale_mtime);
  }

  {
    // A stale session with orphans w/o header
    std::string session_tag = "tag2";
    sync_pb::SessionSpecifics orphan;
    helper()->BuildTabSpecifics(session_tag, kWindowId1, kTabIds1[1], &orphan);
    AddToSyncDataList(orphan, &foreign_data, stale_mtime);
  }

  {
    // A stale session with valid header/tab and an orphaned tab.
    std::string session_tag = "tag3";
    std::vector<sync_pb::SessionSpecifics> tabs;
    sync_pb::SessionSpecifics meta(
        helper()->BuildForeignSession(session_tag, SessionIDs({2}), &tabs));

    // BuildForeignSession(...) will use a window id of 0, and we're also
    // passing a window id of 0 to BuildTabSpecifics(...) here.  It doesn't
    // really matter what window id we use for the orphaned tab, in the real
    // world orphans often reference real/still valid windows, but they're
    // orphans because the window/header doesn't reference back to them.
    sync_pb::SessionSpecifics orphan;
    helper()->BuildTabSpecifics(session_tag, kWindowId1, kTabIds1[1], &orphan);
    AddToSyncDataList(orphan, &foreign_data, stale_mtime);

    AddToSyncDataList(tabs[0], &foreign_data, stale_mtime);
    AddToSyncDataList(orphan, &foreign_data, stale_mtime);
    AddToSyncDataList(meta, &foreign_data, stale_mtime);
  }

  SyncChangeList output;
  AddWindow();
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());
  output.clear();

  // Although we have 3 foreign sessions, only 1 is valid/clean enough.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  foreign_sessions.clear();

  // Everything should get removed here.
  manager()->DoGarbageCollection();
  // Expect 5 deletions. tag1 header only, tag2 tab only, tag3 header + 2x tabs.
  ASSERT_EQ(5U, output.size());
  ASSERT_TRUE(AllOfChangesAreType(output, SyncChange::ACTION_DELETE));
}

// Test that an update to a previously considered "stale" session,
// prior to garbage collection, will save the session from deletion.
TEST_F(SessionsSyncManagerTest, GarbageCollectionHonoursUpdate) {
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, kTabIds1, &tabs1));
  SyncDataList foreign_data;
  base::Time tag1_time = base::Time::Now() - base::TimeDelta::FromDays(21);
  foreign_data.push_back(CreateRemoteData(meta, tag1_time));
  AddTabsToSyncDataList(tabs1, &foreign_data);
  SyncChangeList output;
  AddWindow();
  InitWithSyncDataTakeOutput(foreign_data, &output);
  ASSERT_EQ(2U, output.size());

  // Update to a non-stale time.
  sync_pb::EntitySpecifics update_entity;
  update_entity.mutable_session()->CopyFrom(tabs1[0]);
  SyncChangeList changes;
  changes.push_back(SyncChange(FROM_HERE, SyncChange::ACTION_UPDATE,
                               CreateRemoteData(tabs1[0], base::Time::Now())));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  foreign_sessions.clear();

  // Verify the now non-stale session does not get deleted.
  manager()->DoGarbageCollection();
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID>> session_reference;
  session_reference.push_back(kTabIds1);
  helper()->VerifySyncedSession(kTag1, session_reference,
                                *(foreign_sessions[0]));
}

// Test that NOTIFICATION_FOREIGN_SESSION_UPDATED is sent when processing
// sync changes.
TEST_F(SessionsSyncManagerTest, NotifiedOfUpdates) {
  InitWithNoSyncData();

  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession("tag1", SessionIDs({5}), &tabs1));

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_ADD));
  EXPECT_CALL(*mock_sync_sessions_client(), NotifyForeignSessionUpdated());
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  changes.clear();
  AddTabsToChangeList(tabs1, SyncChange::ACTION_ADD, &changes);
  EXPECT_CALL(*mock_sync_sessions_client(), NotifyForeignSessionUpdated());
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  changes.clear();
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_DELETE));
  EXPECT_CALL(*mock_sync_sessions_client(), NotifyForeignSessionUpdated());
  manager()->ProcessSyncChanges(FROM_HERE, changes);
}

// Test that NOTIFICATION_FOREIGN_SESSION_UPDATED is sent when handling
// local hide/removal of foreign session.
TEST_F(SessionsSyncManagerTest, NotifiedOfLocalRemovalOfForeignSession) {
  InitWithNoSyncData();
  const std::string tag("tag1");
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(tag, SessionIDs({5}), &tabs1));

  SyncChangeList changes;
  changes.push_back(MakeRemoteChange(meta, SyncChange::ACTION_ADD));
  manager()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_CALL(*mock_sync_sessions_client(), NotifyForeignSessionUpdated());
  manager()->GetOpenTabsUIDelegate()->DeleteForeignSession(tag);
}

// Tests receipt of duplicate tab IDs in the same window.  This should never
// happen, but we want to make sure the client won't do anything bad if it does
// receive such garbage input data.
TEST_F(SessionsSyncManagerTest, ReceiveDuplicateTabInSameWindow) {
  std::string tag = "tag1";

  // Reuse tab ID 10 in an attempt to trigger bad behavior.
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(tag, kTabIds1, &tabs1));

  // Set up initial data.
  SyncDataList initial_data;
  sync_pb::EntitySpecifics entity;
  entity.mutable_session()->CopyFrom(meta);
  initial_data.push_back(CreateRemoteData(entity));
  AddTabsToSyncDataList(tabs1, &initial_data);

  SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);
}

// Tests receipt of duplicate tab IDs for the same session.  The duplicate tab
// ID is present in two different windows.  A client can't be expected to do
// anything reasonable with this input, but we can expect that it doesn't
// crash.
TEST_F(SessionsSyncManagerTest, ReceiveDuplicateTabInOtherWindow) {
  // Tab ID 10 is a duplicate.
  const std::vector<SessionID> tab_list1 = SessionIDs({5, 10, 15});
  const std::vector<SessionID> tab_list2 = SessionIDs({7, 10, 17});

  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, tab_list1, &tabs1));

  // Add a second window.  Tab ID 10 is a duplicate.
  helper()->AddWindowSpecifics(kWindowId2, tab_list2, &meta);

  // Set up initial data.
  SyncDataList initial_data;
  sync_pb::EntitySpecifics entity;
  entity.mutable_session()->CopyFrom(meta);
  initial_data.push_back(CreateRemoteData(entity));
  AddTabsToSyncDataList(tabs1, &initial_data);

  for (SessionID tab_id : tab_list2) {
    sync_pb::EntitySpecifics entity;
    helper()->BuildTabSpecifics(kTag1, kWindowId1, tab_id,
                                entity.mutable_session());
    initial_data.push_back(CreateRemoteData(entity));
  }

  SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);
}

// Tests receipt of multiple unassociated tabs and makes sure that
// the ones with later timestamp win
TEST_F(SessionsSyncManagerTest, ReceiveDuplicateUnassociatedTabs) {
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, kTabIds1, &tabs1));

  // Set up initial data.
  SyncDataList initial_data;
  initial_data.push_back(CreateRemoteData(meta));

  sync_pb::EntitySpecifics entity;

  for (size_t i = 0; i < tabs1.size(); ++i) {
    entity.mutable_session()->CopyFrom(tabs1[i]);
    initial_data.push_back(
        CreateRemoteData(entity, base::Time::FromDoubleT(2000)));
  }

  // Add two more tabs with duplicating IDs but with different modification
  // times, one before and one after the tabs above.
  // These two tabs get a different visual indices to distinguish them from the
  // tabs above that get visual index 1 by default.
  sync_pb::SessionSpecifics duplicating_tab1;
  helper()->BuildTabSpecifics(kTag1, kWindowId1, kTabIds1[1],
                              &duplicating_tab1);
  duplicating_tab1.mutable_tab()->set_tab_visual_index(2);
  entity.mutable_session()->CopyFrom(duplicating_tab1);
  initial_data.push_back(
      CreateRemoteData(entity, base::Time::FromDoubleT(1000)));

  sync_pb::SessionSpecifics duplicating_tab2;
  helper()->BuildTabSpecifics(kTag1, kWindowId1, kTabIds1[2],
                              &duplicating_tab2);
  duplicating_tab2.mutable_tab()->set_tab_visual_index(3);
  entity.mutable_session()->CopyFrom(duplicating_tab2);
  initial_data.push_back(
      CreateRemoteData(entity, base::Time::FromDoubleT(3000)));

  SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));

  const std::vector<std::unique_ptr<sessions::SessionTab>>& window_tabs =
      foreign_sessions[0]
          ->windows.find(kWindowId1)
          ->second->wrapped_window.tabs;
  ASSERT_EQ(4U, window_tabs.size());
  // The first one is from the original set of tabs.
  ASSERT_EQ(1, window_tabs[0]->tab_visual_index);
  // The one from the original set of tabs wins over duplicating_tab1.
  ASSERT_EQ(1, window_tabs[1]->tab_visual_index);
  // duplicating_tab2 wins due to the later timestamp.
  ASSERT_EQ(3, window_tabs[2]->tab_visual_index);
}

// Verify that GetAllForeignSessions returns all sessions sorted by recency.
TEST_F(SessionsSyncManagerTest, GetAllForeignSessions) {
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta1(
      helper()->BuildForeignSession(kTag1, kTabIds1, &tabs1));

  std::vector<sync_pb::SessionSpecifics> tabs2;
  sync_pb::SessionSpecifics meta2(
      helper()->BuildForeignSession(kTag2, kTabIds1, &tabs2));

  SyncDataList initial_data;
  initial_data.push_back(
      CreateRemoteData(meta1, base::Time::FromInternalValue(10)));
  AddTabsToSyncDataList(tabs1, &initial_data);
  initial_data.push_back(
      CreateRemoteData(meta2, base::Time::FromInternalValue(200)));
  AddTabsToSyncDataList(tabs2, &initial_data);

  SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);

  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(manager()->GetOpenTabsUIDelegate()->GetAllForeignSessions(
      &foreign_sessions));
  ASSERT_EQ(2U, foreign_sessions.size());
  ASSERT_GT(foreign_sessions[0]->modified_time,
            foreign_sessions[1]->modified_time);
}

// Verify that GetForeignSessionTabs returns all tabs for a session sorted
// by recency.
TEST_F(SessionsSyncManagerTest, GetForeignSessionTabs) {
  std::vector<sync_pb::SessionSpecifics> tabs1;
  sync_pb::SessionSpecifics meta(
      helper()->BuildForeignSession(kTag1, kTabIds1, &tabs1));
  // Add a second window.
  helper()->AddWindowSpecifics(kWindowId2, kTabIds2, &meta);

  // Set up initial data.
  SyncDataList initial_data;
  initial_data.push_back(CreateRemoteData(meta));

  // Add the first window's tabs.
  AddTabsToSyncDataList(tabs1, &initial_data);

  // Add the second window's tabs.
  for (size_t i = 0; i < kTabIds2.size(); ++i) {
    sync_pb::EntitySpecifics entity;
    helper()->BuildTabSpecifics(kTag1, kWindowId1, kTabIds2[i],
                                entity.mutable_session());
    // Order the tabs oldest to most recent and left to right visually.
    initial_data.push_back(
        CreateRemoteData(entity, base::Time::FromInternalValue(i + 1)));
  }

  SyncChangeList output;
  InitWithSyncDataTakeOutput(initial_data, &output);

  std::vector<const sessions::SessionTab*> tabs;
  ASSERT_TRUE(
      manager()->GetOpenTabsUIDelegate()->GetForeignSessionTabs(kTag1, &tabs));
  // Assert that the size matches the total number of tabs and that the order
  // is from most recent to least.
  ASSERT_EQ(kTabIds1.size() + kTabIds2.size(), tabs.size());
  base::Time last_time;
  for (size_t i = 0; i < tabs.size(); ++i) {
    base::Time this_time = tabs[i]->timestamp;
    if (i > 0)
      ASSERT_GE(last_time, this_time);
    last_time = tabs[i]->timestamp;
  }
}

// Ensure model association associates the pre-existing tabs.
TEST_F(SessionsSyncManagerTest, SwappedOutOnRestore) {
  // Start with three tabs in a window.
  TestSyncedWindowDelegate* window = AddWindow();
  TestSyncedTabDelegate* tab1 = AddTab(window->GetSessionId(), kFoo1);
  tab1->Navigate(kFoo2);
  TestSyncedTabDelegate* tab2 = AddTab(window->GetSessionId(), kBar1);
  tab2->Navigate(kBar2);

  SyncDataList in;
  SyncChangeList out;
  InitWithSyncDataTakeOutput(in, &out);

  // Should be one header add, 2 tab adds/updates, one header update.
  ASSERT_EQ(4U, out.size());

  // Now update the sync data to be:
  // * one "normal" fully loaded tab
  // * one placeholder tab with no WebContents and no tab_id change
  sync_pb::EntitySpecifics t0_entity = out[1].sync_data().GetSpecifics();
  sync_pb::EntitySpecifics t1_entity = out[2].sync_data().GetSpecifics();
  in.push_back(CreateRemoteData(t0_entity));
  in.push_back(CreateRemoteData(t1_entity));
  out.clear();
  manager()->StopSyncing(syncer::SESSIONS);
  ResetWindows();

  PlaceholderTabDelegate t1_override(
      SessionID::FromSerializedValue(t1_entity.session().tab().tab_id()));
  window = AddWindow();
  window->OverrideTabAt(0, tab1);
  window->OverrideTabAt(1, &t1_override);
  InitWithSyncDataTakeOutput(in, &out);

  // The last change should be the final header update, reflecting 1 window
  // and 2 tabs.
  VerifyLocalHeaderChange(out.back(), 1, 2);

  // There should be one tab change, for the fully associated tab. The
  // window-ID change for the placeholder tab is not reported to avoid traffic,
  // since nothing relies on it.
  ASSERT_TRUE(AllOfChangesAreType(*FilterOutLocalHeaderChanges(&out),
                                  SyncChange::ACTION_UPDATE));
  ASSERT_EQ(1U, out.size());
  VerifyLocalTabChange(out[0], 2, kFoo2);
}

// Ensure model association does not update the window ID for placeholder tabs.
TEST_F(SessionsSyncManagerTest, WindowIdNotUpdatedOnRestoreForPlaceholderTab) {
  SyncDataList in;
  SyncChangeList out;

  // Set up one tab and start sync with it.
  TestSyncedWindowDelegate* window = AddWindow();
  AddTab(window->GetSessionId(), kFoo1);
  InitWithSyncDataTakeOutput(in, &out);

  // Should be one header add, 1 tab add, and one header update.
  ASSERT_EQ(3U, out.size());
  const sync_pb::EntitySpecifics t0_entity = out[1].sync_data().GetSpecifics();
  ASSERT_TRUE(t0_entity.session().has_tab());

  in.push_back(CreateRemoteData(t0_entity));
  out.clear();
  manager()->StopSyncing(syncer::SESSIONS);
  ResetWindows();

  // Override the tab with a placeholder tab delegate.
  PlaceholderTabDelegate t0_override(
      SessionID::FromSerializedValue(t0_entity.session().tab().tab_id()));

  // Set up the window with the new window ID and placeholder tab.
  window = AddWindow();
  window->OverrideTabAt(0, &t0_override);
  InitWithSyncDataTakeOutput(in, &out);

  // There should be no change other than the header update.
  ASSERT_EQ(0U, FilterOutLocalHeaderChanges(&out)->size());
}

// Ensure that the manager properly ignores a restored placeholder that refers
// to a tab node that doesn't exist
TEST_F(SessionsSyncManagerTest, RestoredPlacholderTabNodeDeleted) {
  syncer::SyncDataList in;
  syncer::SyncChangeList out;

  // Set up one tab and start sync with it.
  TestSyncedWindowDelegate* window = AddWindow();
  AddTab(window->GetSessionId(), kFoo1);
  InitWithSyncDataTakeOutput(in, &out);

  // Should be one header add, 1 tab add, and one header update.
  ASSERT_EQ(3U, out.size());
  const sync_pb::EntitySpecifics t0_entity = out[1].sync_data().GetSpecifics();
  ASSERT_TRUE(t0_entity.session().has_tab());

  out.clear();
  manager()->StopSyncing(syncer::SESSIONS);

  // Override the tab with a placeholder tab delegate.
  PlaceholderTabDelegate t0_override(
      SessionID::FromSerializedValue(t0_entity.session().tab().tab_id()));

  // Override the tab with a placeholder whose sync entity won't exist.
  window->OverrideTabAt(0, &t0_override);
  InitWithSyncDataTakeOutput(in, &out);

  // Because no entities were passed in at associate time, there should be no
  // tab changes.
  ASSERT_EQ(0U, FilterOutLocalHeaderChanges(&out)->size());
}

// Tests that task ids are generated for navigations on local tabs.
TEST_F(SessionsSyncManagerTest, TrackTasksOnLocalTabModified) {
  SyncChangeList changes;
  TestSyncedWindowDelegate* window = AddWindow();
  InitWithSyncDataTakeOutput(SyncDataList(), &changes);
  SessionID window_id = window->GetSessionId();
  ASSERT_FALSE(manager()->current_machine_tag().empty());
  changes.clear();

  // Tab 1
  AddTab(window_id, kFoo1)
      ->Navigate(kFoo2, base::Time::Now(), ui::PAGE_TRANSITION_TYPED);
  // Tab 2
  AddTab(window_id, kBar1)
      ->Navigate(kBar2, base::Time::Now(), ui::PAGE_TRANSITION_LINK);

  // We only test changes for tab add and tab update, and ignore header updates.
  FilterOutLocalHeaderChanges(&changes);
  // Sync data of adding Tab 1 change
  sync_pb::SessionTab tab =
      SyncDataLocal(changes[0].sync_data()).GetSpecifics().session().tab();
  EXPECT_EQ(tab.navigation_size(), 1);
  EXPECT_EQ(tab.navigation(0).global_id(), tab.navigation(0).task_id());
  EXPECT_TRUE(tab.navigation(0).ancestor_task_id().empty());

  // Sync data of updating Tab 1 change
  tab = SyncDataLocal(changes[1].sync_data()).GetSpecifics().session().tab();
  EXPECT_EQ(tab.navigation_size(), 2);
  // navigation(0) and navigation(1) are two separated tasks.
  EXPECT_EQ(tab.navigation(0).global_id(), tab.navigation(0).task_id());
  EXPECT_TRUE(tab.navigation(0).ancestor_task_id().empty());
  EXPECT_EQ(tab.navigation(1).global_id(), tab.navigation(1).task_id());
  EXPECT_TRUE(tab.navigation(1).ancestor_task_id().empty());

  // Sync data of adding Tab 2 change
  tab = SyncDataLocal(changes[2].sync_data()).GetSpecifics().session().tab();
  EXPECT_EQ(tab.navigation_size(), 1);
  EXPECT_EQ(tab.navigation(0).global_id(), tab.navigation(0).task_id());
  EXPECT_TRUE(tab.navigation(0).ancestor_task_id().empty());

  // Sync data of updating Tab 2 change
  tab = SyncDataLocal(changes[3].sync_data()).GetSpecifics().session().tab();
  EXPECT_EQ(tab.navigation_size(), 2);
  EXPECT_EQ(tab.navigation(0).global_id(), tab.navigation(0).task_id());
  EXPECT_TRUE(tab.navigation(0).ancestor_task_id().empty());
  EXPECT_EQ(tab.navigation(1).global_id(), tab.navigation(1).task_id());
  // navigation(1) is a subtask of navigation(0).
  EXPECT_EQ(tab.navigation(1).ancestor_task_id_size(), 1);
  EXPECT_EQ(tab.navigation(1).ancestor_task_id(0), tab.navigation(0).task_id());
}

}  // namespace sync_sessions
