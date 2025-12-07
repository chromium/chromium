// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session_tracker.h"

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/test_matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::Ne;
using testing::NotNull;
using testing::Pointee;
using testing::Return;

namespace sync_sessions {

namespace {

const char kValidUrl[] = "http://www.example.com";
const char kSessionName[] = "sessionname";
// Monday, September 2, 2024 13:31:31 GMT+2.
const base::Time kSessionStartTime =
    base::Time::FromSecondsSinceUnixEpoch(1725283891);
const sync_pb::SyncEnums::DeviceType kDeviceType =
    sync_pb::SyncEnums_DeviceType_TYPE_PHONE;
const syncer::DeviceInfo::FormFactor kFormFactor =
    syncer::DeviceInfo::FormFactor::kPhone;
const char kTag[] = "tag";
const char kTag2[] = "tag2";
const char kTag3[] = "tag3";
const int kTabNode1 = 0;
const int kTabNode2 = 1;
const int kTabNode3 = 2;
const int kTabNode4 = 3;
const int kTabNode5 = 4;
const SessionID kWindow1 = SessionID::FromSerializedValue(1);
const SessionID kWindow2 = SessionID::FromSerializedValue(2);
const SessionID kWindow3 = SessionID::FromSerializedValue(3);
const SessionID kTab1 = SessionID::FromSerializedValue(15);
const SessionID kTab2 = SessionID::FromSerializedValue(25);
const SessionID kTab3 = SessionID::FromSerializedValue(35);
const SessionID kTab4 = SessionID::FromSerializedValue(45);
const SessionID kTab5 = SessionID::FromSerializedValue(55);
const SessionID kTab6 = SessionID::FromSerializedValue(65);
const SessionID kTab7 = SessionID::FromSerializedValue(75);

MATCHER_P(HasSessionTag, expected_tag, "") {
  return arg->GetSessionTag() == expected_tag;
}

}  // namespace

class SyncedSessionTrackerTest : public testing::Test {
 public:
  SyncedSessionTrackerTest() : tracker_(&sessions_client_) {}
  ~SyncedSessionTrackerTest() override = default;

  TabNodePool* GetLocalTabNodePool() {
    return &tracker_.LookupTrackedSession(tracker_.local_session_tag_)
                ->tab_node_pool;
  }

  // Returns whether |tab_node_id| refers to a valid tab node that is associated
  // with a tab.
  bool IsLocalTabNodeAssociated(int tab_node_id) const {
    return tracker_
        .LookupTabIdFromTabNodeId(tracker_.local_session_tag_, tab_node_id)
        .is_valid();
  }

  // Verify that each tab within a session is allocated one SessionTab object,
  // and that that tab object is owned either by the Session itself or the
  // |unmapped_tabs_| tab holder.
  AssertionResult VerifyTabIntegrity(const std::string& session_tag) {
    const SyncedSessionTracker::TrackedSession* session =
        tracker_.LookupTrackedSession(session_tag);
    if (!session) {
      return AssertionFailure()
             << "Not tracked session with tag " << session_tag;
    }

    // First get all the tabs associated with this session.
    int total_tab_count = session->synced_tab_map.size();

    // Now traverse the SyncedSession tree to verify the mapped tabs all match
    // up.
    int mapped_tab_count = 0;
    for (auto& [window_id, window] : session->synced_session.windows) {
      mapped_tab_count += window->wrapped_window.tabs.size();
      for (auto& tab : window->wrapped_window.tabs) {
        const auto tab_map_it = session->synced_tab_map.find(tab->tab_id);
        if (tab_map_it == session->synced_tab_map.end()) {
          return AssertionFailure() << "Tab ID " << tab->tab_id.id()
                                    << " has no corresponding synced tab entry";
        }
        if (tab_map_it->second != tab.get()) {
          return AssertionFailure()
                 << "Mapped tab " << tab->tab_id.id()
                 << " does not match synced tab map " << tab->tab_id.id();
        }
      }
    }

    // Wrap up by verifying all unmapped tabs are tracked.
    int unmapped_tab_count = session->unmapped_tabs.size();
    for (const auto& [id, tab] : session->unmapped_tabs) {
      if (id != tab->tab_id) {
        return AssertionFailure() << "Unmapped tab " << tab->tab_id.id()
                                  << " associated with wrong tab " << id;
      }
      const auto tab_map_it = session->synced_tab_map.find(tab->tab_id);
      if (tab_map_it == session->synced_tab_map.end()) {
        return AssertionFailure() << "Unmapped tab " << tab->tab_id
                                  << " has no corresponding synced tab entry";
      }
      if (tab_map_it->second != tab.get()) {
        return AssertionFailure()
               << "Unmapped tab " << tab->tab_id.id()
               << " does not match synced tab map " << tab_map_it->second;
      }
    }

    return mapped_tab_count + unmapped_tab_count == total_tab_count
               ? AssertionSuccess()
               : AssertionFailure()
                     << " Tab count mismatch. Total: " << total_tab_count
                     << ". Mapped + Unmapped: " << mapped_tab_count << " + "
                     << unmapped_tab_count;
  }

 protected:
  testing::NiceMock<MockSyncSessionsClient> sessions_client_;
  testing::NiceMock<
      base::MockCallback<base::RepeatingCallback<bool(int /*tab_node_id*/)>>>
      is_tab_node_unsynced_cb_;
  SyncedSessionTracker tracker_;
};

TEST_F(SyncedSessionTrackerTest, GetSession) {
  SyncedSession* session1 = tracker_.GetSession(kTag);
  SyncedSession* session2 = tracker_.GetSession(kTag2);
  ASSERT_EQ(session1, tracker_.LookupSession(kTag));
  ASSERT_EQ(session1, tracker_.GetSession(kTag));
  ASSERT_NE(session1, session2);
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, GetTabUnmapped) {
  sessions::SessionTab* tab = tracker_.GetTab(kTag, kTab1);
  ASSERT_EQ(tab, tracker_.GetTab(kTag, kTab1));
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, PutWindowInSession) {
  tracker_.PutWindowInSession(kTag, kWindow1);
  const SyncedSession* session = tracker_.LookupSession(kTag);
  ASSERT_EQ(1U, session->windows.size());

  // Doing it again should have no effect.
  tracker_.PutWindowInSession(kTag, kWindow1);
  ASSERT_EQ(1U, session->windows.size());
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, PutTabInWindow) {
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  const SyncedSession* session = tracker_.LookupSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows.at(kWindow1)->wrapped_window.tabs.size());
  ASSERT_EQ(tracker_.GetTab(kTag, kTab1),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, LookupAllSessions) {
  EXPECT_THAT(tracker_.LookupAllSessions(SyncedSessionTracker::PRESENTABLE),
              IsEmpty());

  tracker_.InitLocalSession(kTag, kSessionName, kDeviceType, kFormFactor);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);

  EXPECT_THAT(tracker_.LookupAllSessions(SyncedSessionTracker::RAW),
              ElementsAre(HasSessionTag(kTag)));
  EXPECT_THAT(tracker_.LookupAllSessions(SyncedSessionTracker::PRESENTABLE),
              IsEmpty());

  sessions::SessionTab* tab = tracker_.GetTab(kTag, kTab1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest());
  EXPECT_THAT(tracker_.LookupAllSessions(SyncedSessionTracker::PRESENTABLE),
              ElementsAre(HasSessionTag(kTag)));

  tracker_.GetSession(kTag2);
  tracker_.PutWindowInSession(kTag2, kWindow1);
  tracker_.PutTabInWindow(kTag2, kWindow1, kTab2);

  sessions::SessionTab* tab2 = tracker_.GetTab(kTag2, kTab2);
  ASSERT_TRUE(tab2);
  tab2->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest());
  EXPECT_THAT(tracker_.LookupAllSessions(SyncedSessionTracker::PRESENTABLE),
              ElementsAre(HasSessionTag(kTag), HasSessionTag(kTag2)));
}

TEST_F(SyncedSessionTrackerTest, LookupAllForeignSessions) {
  const char kInvalidUrl[] = "invalid.url";
  ON_CALL(sessions_client_, ShouldSyncURL(GURL(kInvalidUrl)))
      .WillByDefault(testing::Return(false));

  EXPECT_THAT(
      tracker_.LookupAllForeignSessions(SyncedSessionTracker::PRESENTABLE),
      IsEmpty());
  tracker_.GetSession(kTag);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  sessions::SessionTab* tab = tracker_.GetTab(kTag, kTab1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest());
  tab->navigations.back().set_virtual_url(GURL(kValidUrl));
  tracker_.GetSession(kTag2);
  tracker_.GetSession(kTag3);
  tracker_.PutWindowInSession(kTag3, kWindow1);
  tracker_.PutTabInWindow(kTag3, kWindow1, kTab1);
  tab = tracker_.GetTab(kTag3, kTab1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest());
  tab->navigations.back().set_virtual_url(GURL(kInvalidUrl));

  // Only the session with a valid window and tab gets returned.
  EXPECT_THAT(
      tracker_.LookupAllForeignSessions(SyncedSessionTracker::PRESENTABLE),
      ElementsAre(HasSessionTag(kTag)));
  EXPECT_THAT(tracker_.LookupAllForeignSessions(SyncedSessionTracker::RAW),
              ElementsAre(HasSessionTag(kTag), HasSessionTag(kTag2),
                          HasSessionTag(kTag3)));

  // Annotate kTag as local session.
  ON_CALL(sessions_client_, IsRecentLocalCacheGuid(kTag))
      .WillByDefault(Return(true));
  EXPECT_THAT(
      tracker_.LookupAllForeignSessions(SyncedSessionTracker::PRESENTABLE),
      IsEmpty());
  EXPECT_THAT(tracker_.LookupAllForeignSessions(SyncedSessionTracker::RAW),
              ElementsAre(HasSessionTag(kTag2), HasSessionTag(kTag3)));
}

TEST_F(SyncedSessionTrackerTest, LookupSessionWindows) {
  std::vector<const sessions::SessionWindow*> windows =
      tracker_.LookupSessionWindows(kTag);
  EXPECT_TRUE(windows.empty());
  tracker_.GetSession(kTag);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutWindowInSession(kTag, kWindow2);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  tracker_.PutTabInWindow(kTag, kWindow2, kTab2);
  tracker_.GetSession(kTag2);
  tracker_.PutWindowInSession(kTag2, kWindow1);
  tracker_.PutWindowInSession(kTag2, kWindow2);
  windows = tracker_.LookupSessionWindows(kTag);
  ASSERT_EQ(2U, windows.size());  // Only windows from kTag session.
  ASSERT_NE((sessions::SessionWindow*)nullptr, windows[0]);
  ASSERT_NE((sessions::SessionWindow*)nullptr, windows[1]);
  ASSERT_NE(windows[1], windows[0]);
}

TEST_F(SyncedSessionTrackerTest, LookupSessionTab) {
  ASSERT_THAT(tracker_.LookupSessionTab(kTag, SessionID::InvalidValue()),
              IsNull());
  ASSERT_THAT(tracker_.LookupSessionTab(kTag, kTab1), IsNull());
  tracker_.GetSession(kTag);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  ASSERT_THAT(tracker_.LookupSessionTab(kTag, kTab1), NotNull());
}

TEST_F(SyncedSessionTrackerTest, Complex) {
  std::vector<sessions::SessionTab*> tabs1, tabs2;
  sessions::SessionTab* temp_tab;
  ASSERT_TRUE(tracker_.Empty());
  ASSERT_EQ(0U, tracker_.num_synced_sessions());
  ASSERT_EQ(0U, tracker_.num_synced_tabs(kTag));
  tabs1.push_back(tracker_.GetTab(kTag, kTab1));
  tabs1.push_back(tracker_.GetTab(kTag, kTab2));
  tabs1.push_back(tracker_.GetTab(kTag, kTab3));
  ASSERT_EQ(3U, tracker_.num_synced_tabs(kTag));
  ASSERT_EQ(1U, tracker_.num_synced_sessions());
  temp_tab = tracker_.GetTab(kTag, kTab1);  // Already created.
  ASSERT_EQ(3U, tracker_.num_synced_tabs(kTag));
  ASSERT_EQ(1U, tracker_.num_synced_sessions());
  ASSERT_EQ(tabs1[0], temp_tab);
  tabs2.push_back(tracker_.GetTab(kTag2, kTab1));
  ASSERT_EQ(1U, tracker_.num_synced_tabs(kTag2));
  ASSERT_EQ(2U, tracker_.num_synced_sessions());
  tracker_.DeleteForeignSession(kTag3);

  SyncedSession* session = tracker_.GetSession(kTag);
  ASSERT_EQ(2U, tracker_.num_synced_sessions());
  SyncedSession* session2 = tracker_.GetSession(kTag2);
  ASSERT_EQ(2U, tracker_.num_synced_sessions());
  SyncedSession* session3 = tracker_.GetSession(kTag3);
  session3->SetDeviceTypeAndFormFactor(
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
      syncer::DeviceInfo::FormFactor::kDesktop);
  ASSERT_EQ(3U, tracker_.num_synced_sessions());

  ASSERT_TRUE(session);
  ASSERT_TRUE(session2);
  ASSERT_TRUE(session3);
  ASSERT_NE(session, session2);
  ASSERT_NE(session2, session3);
  tracker_.DeleteForeignSession(kTag3);
  ASSERT_EQ(2U, tracker_.num_synced_sessions());

  tracker_.PutWindowInSession(kTag, kWindow1);     // Create a window.
  tracker_.PutTabInWindow(kTag, kWindow1, kTab3);  // No longer unmapped.
  ASSERT_EQ(3U, tracker_.num_synced_tabs(kTag));   // Has not changed.

  ASSERT_EQ(tabs1[0], tracker_.LookupSessionTab(kTag, kTab1));
  ASSERT_EQ(tabs1[2], tracker_.LookupSessionTab(kTag, kTab3));
  ASSERT_THAT(tracker_.LookupSessionTab(kTag, kTab4), IsNull());

  std::vector<const sessions::SessionWindow*> windows =
      tracker_.LookupSessionWindows(kTag);
  ASSERT_EQ(1U, windows.size());
  windows = tracker_.LookupSessionWindows(kTag2);
  ASSERT_EQ(0U, windows.size());

  // The sessions don't have valid tabs, lookup should not succeed.
  std::vector<const SyncedSession*> sessions;
  EXPECT_THAT(
      tracker_.LookupAllForeignSessions(SyncedSessionTracker::PRESENTABLE),
      IsEmpty());
  EXPECT_THAT(tracker_.LookupAllForeignSessions(SyncedSessionTracker::RAW),
              ElementsAre(HasSessionTag(kTag), HasSessionTag(kTag2)));

  tracker_.Clear();
  ASSERT_EQ(0U, tracker_.num_synced_tabs(kTag));
  ASSERT_EQ(0U, tracker_.num_synced_tabs(kTag2));
  ASSERT_EQ(0U, tracker_.num_synced_sessions());
}

TEST_F(SyncedSessionTrackerTest, ManyGetTabs) {
  ASSERT_TRUE(tracker_.Empty());
  const int kMaxSessions = 10;
  const int kMaxTabs = 1000;
  const int kMaxAttempts = 10000;
  for (int j = 0; j < kMaxSessions; ++j) {
    std::string tag = base::StringPrintf("tag%d", j);
    for (int i = 0; i < kMaxAttempts; ++i) {
      // More attempts than tabs means we'll sometimes get the same tabs,
      // sometimes have to allocate new tabs.
      int rand_tab_num = base::RandInt(0, kMaxTabs);
      sessions::SessionTab* tab = tracker_.GetTab(
          tag, SessionID::FromSerializedValue(rand_tab_num + 1));
      ASSERT_TRUE(tab);
    }
  }
}

TEST_F(SyncedSessionTrackerTest, LookupTabNodeIds) {
  tracker_.OnTabNodeSeen(kTag, 1, kTab1);
  tracker_.OnTabNodeSeen(kTag, 2, kTab2);
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag), ElementsAre(1, 2));
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag2), IsEmpty());

  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag), ElementsAre(1, 2));

  tracker_.OnTabNodeSeen(kTag, 3, kTab3);
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag), ElementsAre(1, 2, 3));

  tracker_.OnTabNodeSeen(kTag2, 21, kTab4);
  tracker_.OnTabNodeSeen(kTag2, 22, kTab5);
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag2), ElementsAre(21, 22));
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag), ElementsAre(1, 2, 3));

  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag3), IsEmpty());
  tracker_.PutWindowInSession(kTag3, kWindow2);
  tracker_.PutTabInWindow(kTag3, kWindow2, kTab2);
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag3), IsEmpty());
  tracker_.DeleteForeignSession(kTag3);
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag3), IsEmpty());

  tracker_.DeleteForeignSession(kTag);
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag), IsEmpty());
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag2), ElementsAre(21, 22));

  tracker_.OnTabNodeSeen(kTag2, 21, kTab6);
  tracker_.OnTabNodeSeen(kTag2, 23, kTab7);
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag2), ElementsAre(21, 22, 23));

  tracker_.DeleteForeignSession(kTag2);
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag2), IsEmpty());
}

TEST_F(SyncedSessionTrackerTest, SessionTracking) {
  ASSERT_TRUE(tracker_.Empty());

  // Create some session information that is stale.
  SyncedSession* session1 = tracker_.GetSession(kTag);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab2);
  tracker_.GetTab(kTag, kTab3)->window_id =
      SessionID::FromSerializedValue(1);  // Unmapped.
  tracker_.GetTab(kTag, kTab4)->window_id =
      SessionID::FromSerializedValue(1);  // Unmapped.
  tracker_.PutWindowInSession(kTag, kWindow2);
  tracker_.PutTabInWindow(kTag, kWindow2, kTab5);
  tracker_.PutTabInWindow(kTag, kWindow2, kTab6);
  ASSERT_EQ(2U, session1->windows.size());
  ASSERT_EQ(2U, session1->windows[kWindow1]->wrapped_window.tabs.size());
  ASSERT_EQ(2U, session1->windows[kWindow2]->wrapped_window.tabs.size());
  ASSERT_EQ(6U, tracker_.num_synced_tabs(kTag));

  // Create a session that should not be affected.
  SyncedSession* session2 = tracker_.GetSession(kTag2);
  tracker_.PutWindowInSession(kTag2, kWindow3);
  tracker_.PutTabInWindow(kTag2, kWindow3, kTab2);
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[kWindow3]->wrapped_window.tabs.size());
  ASSERT_EQ(1U, tracker_.num_synced_tabs(kTag2));

  // Reset tracking and get the current windows/tabs.
  // We simulate moving a tab from one window to another, then closing the
  // first window (including its one remaining tab), and opening a new tab
  // on the remaining window.

  // New tab, arrived before meta node so unmapped.
  tracker_.GetTab(kTag, kTab7);
  tracker_.ResetSessionTracking(kTag);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  // Tab 1 is closed.
  tracker_.PutTabInWindow(kTag, kWindow1, kTab3);  // No longer unmapped.
  // Tab 3 was unmapped and does not get used.
  tracker_.PutTabInWindow(kTag, kWindow1, kTab5);  // Moved from window 1.
  // Window 1 was closed, along with tab 5.
  tracker_.PutTabInWindow(kTag, kWindow1, kTab7);  // No longer unmapped.
  // Session 2 should not be affected.
  tracker_.CleanupSession(kTag);

  // Verify that only those parts of the session not owned have been removed.
  ASSERT_EQ(1U, session1->windows.size());
  ASSERT_EQ(4U, session1->windows[kWindow1]->wrapped_window.tabs.size());
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[kWindow3]->wrapped_window.tabs.size());
  ASSERT_EQ(2U, tracker_.num_synced_sessions());
  ASSERT_EQ(4U, tracker_.num_synced_tabs(kTag));
  ASSERT_EQ(1U, tracker_.num_synced_tabs(kTag2));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));

  // All memory should be properly deallocated by destructor for the
  // SyncedSessionTracker.
}

TEST_F(SyncedSessionTrackerTest, DeleteForeignTab) {
  int tab_node_id_1 = 1;
  int tab_node_id_2 = 2;
  std::set<int> result;

  tracker_.OnTabNodeSeen(kTag, tab_node_id_1, kTab1);
  tracker_.OnTabNodeSeen(kTag, tab_node_id_2, kTab2);

  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag),
              ElementsAre(tab_node_id_1, tab_node_id_2));

  tracker_.DeleteForeignTab(kTag, tab_node_id_1);
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag), ElementsAre(tab_node_id_2));

  tracker_.DeleteForeignTab(kTag, tab_node_id_2);
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag), IsEmpty());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, CleanupLocalTabs) {
  tracker_.InitLocalSession(kTag, kSessionName, kDeviceType, kFormFactor);

  // Start with four restored tab nodes, one of which is mapped (|kTab1|).
  tracker_.ReassociateLocalTab(kTabNode1, kTab1);
  tracker_.ReassociateLocalTab(kTabNode2, kTab2);
  tracker_.ReassociateLocalTab(kTabNode3, kTab3);
  tracker_.ReassociateLocalTab(kTabNode4, kTab4);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);

  // Modification times are very recent except for one tab, |kTab3|, which is
  // old, and hence is expected to be recycled immediately, even if it's
  // unsynced.
  tracker_.GetTab(kTag, kTab1)->timestamp = base::Time::Now();
  tracker_.GetTab(kTag, kTab2)->timestamp = base::Time::Now();
  tracker_.GetTab(kTag, kTab3)->timestamp = base::Time::Now() - base::Days(100);
  tracker_.GetTab(kTag, kTab4)->timestamp = base::Time::Now();

  // Among the unmapped (closed) ones, |kTab2| and |kTab3| are unsynced.
  // However, |kTab3| is old so it's irrelevant whether it's unsynced and the
  // callback shouldn't even run.
  EXPECT_CALL(is_tab_node_unsynced_cb_, Run(kTabNode2)).WillOnce(Return(true));
  EXPECT_CALL(is_tab_node_unsynced_cb_, Run(kTabNode3)).Times(0);
  EXPECT_CALL(is_tab_node_unsynced_cb_, Run(kTabNode4)).WillOnce(Return(false));

  // During cleanup, only two tabs should be freed:
  // - |kTabNode3| because of its age, although it's unsynced.
  // - |kTabNode4| because it's synced.
  EXPECT_THAT(tracker_.CleanupLocalTabs(is_tab_node_unsynced_cb_.Get()),
              ElementsAre(kTabNode3, kTabNode4));
  ASSERT_EQ(kTabNode1, tracker_.LookupTabNodeFromTabId(kTag, kTab1));
  EXPECT_EQ(kTabNode2, tracker_.LookupTabNodeFromTabId(kTag, kTab2));
  EXPECT_EQ(TabNodePool::kInvalidTabNodeID,
            tracker_.LookupTabNodeFromTabId(kTag, kTab3));
  EXPECT_EQ(TabNodePool::kInvalidTabNodeID,
            tracker_.LookupTabNodeFromTabId(kTag, kTab4));

  // |kTabNode2| now becomes synced (commit succeeded), which means it should be
  // freed during cleanup.
  EXPECT_CALL(is_tab_node_unsynced_cb_, Run(kTabNode2)).WillOnce(Return(false));
  EXPECT_THAT(tracker_.CleanupLocalTabs(is_tab_node_unsynced_cb_.Get()),
              ElementsAre(kTabNode2));
  EXPECT_EQ(TabNodePool::kInvalidTabNodeID,
            tracker_.LookupTabNodeFromTabId(kTag, kTab2));
  EXPECT_EQ(kTabNode2, tracker_.AssociateLocalTabWithFreeTabNode(kTab5));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabMapped) {
  // First create the tab normally.
  tracker_.InitLocalSession(kTag, kSessionName, kDeviceType, kFormFactor);
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode1));
  tracker_.ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(tracker_.IsTabUnmappedForTesting(kTab1));

  // Map it to a window with the same tab id as it was created with.
  tracker_.ResetSessionTracking(kTag);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  tracker_.CleanupLocalTabs(is_tab_node_unsynced_cb_.Get());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));
  const SyncedSession* session = tracker_.LookupSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows.at(kWindow1)->wrapped_window.tabs.size());
  ASSERT_EQ(tracker_.GetTab(kTag, kTab1),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());

  // Then reassociate with a new tab id.
  tracker_.ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab2));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));

  // Reset tracking, and put the new tab id into the window.
  tracker_.ResetSessionTracking(kTag);
  EXPECT_TRUE(tracker_.IsTabUnmappedForTesting(kTab2));
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab2);
  EXPECT_TRUE(
      tracker_.CleanupLocalTabs(is_tab_node_unsynced_cb_.Get()).empty());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  ASSERT_EQ(tracker_.GetTab(kTag, kTab2),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());
  ASSERT_EQ(tracker_.LookupTabNodeIds(kTag).size(),
            tracker_.LookupTabNodeIds(kTag).count(kTabNode1));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabMappedTwice) {
  // First create the tab normally.
  tracker_.InitLocalSession(kTag, kSessionName, kDeviceType, kFormFactor);
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode1));
  tracker_.ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(tracker_.IsTabUnmappedForTesting(kTab1));

  // Map it to a window with the same tab id as it was created with.
  tracker_.ResetSessionTracking(kTag);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  EXPECT_TRUE(
      tracker_.CleanupLocalTabs(is_tab_node_unsynced_cb_.Get()).empty());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));
  const SyncedSession* session = tracker_.LookupSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows.at(kWindow1)->wrapped_window.tabs.size());
  EXPECT_EQ(tracker_.GetTab(kTag, kTab1),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());

  // Then reassociate with a new tab id.
  tracker_.ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab2));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));

  // Tab 1 should no longer be associated with any SessionTab object. At this
  // point there's no need to verify it's unmapped state.
  EXPECT_THAT(tracker_.LookupSessionTab(kTag, kTab1), IsNull());

  // Reset tracking and add back both the old tab and the new tab (both of which
  // refer to the same tab node id).
  tracker_.ResetSessionTracking(kTag);
  EXPECT_TRUE(tracker_.IsTabUnmappedForTesting(kTab2));
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab2);
  EXPECT_TRUE(
      tracker_.CleanupLocalTabs(is_tab_node_unsynced_cb_.Get()).empty());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  EXPECT_EQ(tracker_.GetTab(kTag, kTab2),
            session->windows.at(kWindow1)->wrapped_window.tabs[1].get());
  EXPECT_EQ(tracker_.LookupTabNodeIds(kTag).size(),
            tracker_.LookupTabNodeIds(kTag).count(kTabNode1));

  // Attempting to access the original tab will create a new SessionTab object.
  EXPECT_NE(tracker_.GetTab(kTag, kTab1), tracker_.GetTab(kTag, kTab2));
  EXPECT_EQ(TabNodePool::kInvalidTabNodeID,
            tracker_.LookupTabNodeFromTabId(kTag, kTab1));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabUnmapped) {
  // First create the old tab in an unmapped state.
  tracker_.InitLocalSession(kTag, kSessionName, kDeviceType, kFormFactor);
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode1));
  tracker_.ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(tracker_.IsTabUnmappedForTesting(kTab1));

  // Map it to a window, but reassociated with a new tab id.
  tracker_.ResetSessionTracking(kTag);
  tracker_.ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(tracker_.IsTabUnmappedForTesting(kTab2));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab2);
  EXPECT_TRUE(
      tracker_.CleanupLocalTabs(is_tab_node_unsynced_cb_.Get()).empty());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as GetTab.
  const SyncedSession* session = tracker_.LookupSession(kTag);
  ASSERT_EQ(tracker_.GetTab(kTag, kTab2),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());
  ASSERT_EQ(tracker_.LookupTabNodeIds(kTag).size(),
            tracker_.LookupTabNodeIds(kTag).count(kTabNode1));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabOldUnmappedNewMapped) {
  // First create the old tab in an unmapped state.
  tracker_.InitLocalSession(kTag, kSessionName, kDeviceType, kFormFactor);
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode1));
  tracker_.ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(tracker_.IsTabUnmappedForTesting(kTab1));

  // Map an unseen tab to a window, then reassociate the existing tab to the
  // mapped tab id.
  tracker_.ResetSessionTracking(kTag);
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab2);
  tracker_.CleanupLocalTabs(is_tab_node_unsynced_cb_.Get());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab2));
  tracker_.ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as GetTab.
  const SyncedSession* session = tracker_.LookupSession(kTag);
  ASSERT_EQ(tracker_.GetTab(kTag, kTab2),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());
  ASSERT_EQ(tracker_.LookupTabNodeIds(kTag).size(),
            tracker_.LookupTabNodeIds(kTag).count(kTabNode1));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabSameTabId) {
  // First create the tab normally.
  tracker_.InitLocalSession(kTag, kSessionName, kDeviceType, kFormFactor);
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode1));
  tracker_.ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(tracker_.IsTabUnmappedForTesting(kTab1));

  // Map it to a window.
  tracker_.ResetSessionTracking(kTag);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  EXPECT_TRUE(
      tracker_.CleanupLocalTabs(is_tab_node_unsynced_cb_.Get()).empty());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));
  const SyncedSession* session = tracker_.LookupSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows.at(kWindow1)->wrapped_window.tabs.size());
  ASSERT_EQ(tracker_.GetTab(kTag, kTab1),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());

  // Reassociate, using the same tab id.
  tracker_.ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));

  // Reset tracking, and put the tab id back into the same window.
  tracker_.ResetSessionTracking(kTag);
  EXPECT_TRUE(tracker_.IsTabUnmappedForTesting(kTab1));
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  EXPECT_TRUE(
      tracker_.CleanupLocalTabs(is_tab_node_unsynced_cb_.Get()).empty());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  ASSERT_EQ(tracker_.GetTab(kTag, kTab1),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());
  ASSERT_EQ(tracker_.LookupTabNodeIds(kTag).size(),
            tracker_.LookupTabNodeIds(kTag).count(kTabNode1));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabOldMappedNewUnmapped) {
  // First create an unmapped tab.
  tracker_.InitLocalSession(kTag, kSessionName, kDeviceType, kFormFactor);
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode1));
  tracker_.ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(tracker_.IsTabUnmappedForTesting(kTab1));

  // Now, map the first one, deleting the second one.
  tracker_.ResetSessionTracking(kTag);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  tracker_.CleanupLocalTabs(is_tab_node_unsynced_cb_.Get());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));
  const SyncedSession* session = tracker_.LookupSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows.at(kWindow1)->wrapped_window.tabs.size());
  ASSERT_EQ(tracker_.GetTab(kTag, kTab1),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());

  // Create a second unmapped tab.
  tracker_.ReassociateLocalTab(kTabNode2, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode2));
  EXPECT_TRUE(tracker_.IsTabUnmappedForTesting(kTab2));

  // Reassociate the second tab with node of the first tab.
  tracker_.ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode2));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab2));

  // Now map the new one.
  tracker_.ResetSessionTracking(kTag);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab2);
  tracker_.CleanupLocalTabs(is_tab_node_unsynced_cb_.Get());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab1));
  EXPECT_FALSE(tracker_.IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  ASSERT_EQ(tracker_.GetTab(kTag, kTab2),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithHeader) {
  sync_pb::SessionSpecifics header;
  header.set_session_tag(kTag);
  header.mutable_header()->set_session_start_time_unix_epoch_millis(
      kSessionStartTime.InMillisecondsSinceUnixEpoch());
  header.mutable_header()->add_window()->set_window_id(kWindow1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab2.id());
  UpdateTrackerWithSpecifics(header, base::Time::Now(), &tracker_);

  EXPECT_THAT(tracker_.LookupSession(kTag)->GetStartTime(),
              Eq(kSessionStartTime));
  EXPECT_THAT(
      tracker_.LookupSession(kTag),
      MatchesSyncedSession(kTag, {{kWindow1.id(), {kTab1.id(), kTab2.id()}}}));
  EXPECT_THAT(tracker_.LookupSessionTab(kTag, kTab1), NotNull());
  EXPECT_THAT(tracker_.LookupSessionTab(kTag, kTab2), NotNull());
}

TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithIdenticalHeader) {
  sync_pb::SessionSpecifics header;
  header.set_session_tag(kTag);
  header.mutable_header()->add_window()->set_window_id(kWindow1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab2.id());
  UpdateTrackerWithSpecifics(header, base::Time::Now(), &tracker_);
  ASSERT_THAT(
      tracker_.LookupSession(kTag),
      MatchesSyncedSession(kTag, {{kWindow1.id(), {kTab1.id(), kTab2.id()}}}));

  // Update with an exact header entity.
  UpdateTrackerWithSpecifics(header, base::Time::Now(), &tracker_);

  EXPECT_THAT(
      tracker_.LookupSession(kTag),
      MatchesSyncedSession(kTag, {{kWindow1.id(), {kTab1.id(), kTab2.id()}}}));
  EXPECT_THAT(tracker_.LookupSessionTab(kTag, kTab1), NotNull());
}

// Verifies that an invalid header (with duplicated tab IDs) is discarded.
TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithHeaderWithDuplicateTabIds) {
  sync_pb::SessionSpecifics header;
  header.set_session_tag(kTag);
  header.mutable_header()->add_window()->set_window_id(kWindow1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab1.id());
  UpdateTrackerWithSpecifics(header, base::Time::Now(), &tracker_);

  EXPECT_THAT(tracker_.LookupSession(kTag),
              MatchesSyncedSession(kTag, /*window_id_to_tabs=*/{}));
}

// Verifies that an invalid tab (with invalid ID) is discarded.
TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithInvalidTab) {
  const int kInvalidTabId = -1;
  sync_pb::SessionSpecifics tab;
  tab.set_session_tag(kTag);
  tab.mutable_tab()->set_tab_id(kInvalidTabId);
  UpdateTrackerWithSpecifics(tab, base::Time::Now(), &tracker_);

  EXPECT_THAT(tracker_.LookupSessionTab(
                  kTag, SessionID::FromSerializedValue(kInvalidTabId)),
              IsNull());
}

// Verifies that an invalid header (with duplicated window IDs) is discarded.
TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithHeaderWithDuplicateWindowId) {
  sync_pb::SessionSpecifics header;
  header.set_session_tag(kTag);
  header.mutable_header()->add_window()->set_window_id(kWindow1.id());
  header.mutable_header()->add_window()->set_window_id(kWindow1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab1.id());
  header.mutable_header()->mutable_window(1)->add_tab(kTab2.id());
  UpdateTrackerWithSpecifics(header, base::Time::Now(), &tracker_);

  EXPECT_THAT(tracker_.LookupSession(kTag),
              MatchesSyncedSession(kTag, /*window_id_to_tabs=*/{}));
}

// Verifies that an invalid header with duplicated window IDs is ignored. It
// specifically tests feeding the same input twice to
// UpdateTrackerWithSpecifics(), as a regression test for crbug.com/803205 to
// verify that it at least doesn't crash.
TEST_F(SyncedSessionTrackerTest,
       UpdateTrackerWithHeaderWithDuplicateWindowIdTwice) {
  sync_pb::SessionSpecifics header;
  header.set_session_tag(kTag);
  header.mutable_header()->add_window()->set_window_id(kWindow1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab1.id());
  header.mutable_header()->add_window()->set_window_id(kWindow1.id());
  header.mutable_header()->mutable_window(1)->add_tab(kTab2.id());
  UpdateTrackerWithSpecifics(header, base::Time::Now(), &tracker_);
  UpdateTrackerWithSpecifics(header, base::Time::Now(), &tracker_);

  EXPECT_THAT(tracker_.LookupSession(kTag),
              MatchesSyncedSession(kTag, /*window_id_to_tabs=*/{}));
}

TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithTab) {
  sync_pb::SessionSpecifics tab;
  tab.set_session_tag(kTag);
  tab.set_tab_node_id(kTabNode1);
  tab.mutable_tab()->set_window_id(kWindow1.id());
  tab.mutable_tab()->set_tab_id(kTab1.id());
  UpdateTrackerWithSpecifics(tab, base::Time::Now(), &tracker_);

  const sessions::SessionTab* tracked_tab =
      tracker_.LookupSessionTab(kTag, kTab1);
  ASSERT_THAT(tracked_tab, NotNull());
  EXPECT_EQ(kWindow1, tracked_tab->window_id);
  EXPECT_EQ(false, tracked_tab->pinned);

  EXPECT_THAT(tracker_.LookupSession(kTag),
              MatchesSyncedSession(kTag, /*window_id_to_tabs*/ {}));

  // Overwrite some attribute.
  tab.mutable_tab()->set_pinned(true);
  UpdateTrackerWithSpecifics(tab, base::Time::Now(), &tracker_);
  tracked_tab = tracker_.LookupSessionTab(kTag, kTab1);
  ASSERT_THAT(tracked_tab, NotNull());
  EXPECT_EQ(kWindow1, tracked_tab->window_id);
  EXPECT_EQ(true, tracked_tab->pinned);

  EXPECT_THAT(tracker_.LookupSession(kTag),
              MatchesSyncedSession(kTag, /*window_id_to_tabs*/ {}));
}

TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithTabThenHeader) {
  sync_pb::SessionSpecifics tab;
  tab.set_session_tag(kTag);
  tab.set_tab_node_id(kTabNode1);
  tab.mutable_tab()->set_window_id(kWindow1.id());
  tab.mutable_tab()->set_tab_id(kTab1.id());
  UpdateTrackerWithSpecifics(tab, base::Time::Now(), &tracker_);

  EXPECT_THAT(tracker_.LookupSession(kTag),
              MatchesSyncedSession(kTag, /*window_id_to_tabs*/ {}));

  sync_pb::SessionSpecifics header;
  header.set_session_tag(kTag);
  header.mutable_header()->add_window()->set_window_id(kWindow1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab1.id());
  UpdateTrackerWithSpecifics(header, base::Time::Now(), &tracker_);

  const sessions::SessionTab* tracked_tab =
      tracker_.LookupSessionTab(kTag, kTab1);
  ASSERT_THAT(tracked_tab, NotNull());
  EXPECT_EQ(kWindow1, tracked_tab->window_id);

  EXPECT_THAT(tracker_.LookupSession(kTag),
              MatchesSyncedSession(
                  kTag, {{kWindow1.id(), std::vector<int>{kTab1.id()}}}));
}

TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithTwoTabsSameId) {
  sync_pb::SessionSpecifics tab1;
  tab1.set_session_tag(kTag);
  tab1.set_tab_node_id(kTabNode1);
  tab1.mutable_tab()->set_window_id(kWindow1.id());
  tab1.mutable_tab()->set_tab_id(kTab1.id());
  tab1.mutable_tab()->set_pinned(false);
  UpdateTrackerWithSpecifics(tab1, base::Time::Now(), &tracker_);

  sync_pb::SessionSpecifics tab2;
  tab2.set_session_tag(kTag);
  tab2.set_tab_node_id(kTabNode2);
  tab2.mutable_tab()->set_window_id(kWindow1.id());
  tab2.mutable_tab()->set_tab_id(kTab1.id());
  tab2.mutable_tab()->set_pinned(true);
  UpdateTrackerWithSpecifics(tab2, base::Time::Now(), &tracker_);

  EXPECT_THAT(tracker_.LookupSession(kTag),
              MatchesSyncedSession(kTag, /*window_id_to_tabs*/ {}));
  EXPECT_THAT(tracker_.LookupTabNodeIds(kTag),
              ElementsAre(kTabNode1, kTabNode2));

  const sessions::SessionTab* tracked_tab =
      tracker_.LookupSessionTab(kTag, kTab1);
  ASSERT_THAT(tracked_tab, NotNull());
  EXPECT_EQ(kWindow1, tracked_tab->window_id);
  EXPECT_EQ(true, tracked_tab->pinned);
}

TEST_F(SyncedSessionTrackerTest, SerializeTrackerToSpecifics) {
  tracker_.InitLocalSession(kTag, kSessionName, kDeviceType, kFormFactor);
  tracker_.SetLocalSessionStartTime(kSessionStartTime);
  tracker_.PutWindowInSession(kTag, kWindow1);
  tracker_.GetSession(kTag)->windows[kWindow1]->window_type =
      sync_pb::SyncEnums_BrowserType_TYPE_TABBED;
  tracker_.PutTabInWindow(kTag, kWindow1, kTab1);
  tracker_.PutTabInWindow(kTag, kWindow1, kTab2);
  // Unmapped tab.
  tracker_.GetTab(kTag, kTab3);
  // |kTabNode4| will be unassociated, because |kTab1| is associated twice.
  tracker_.ReassociateLocalTab(kTabNode4, kTab1);
  tracker_.ReassociateLocalTab(kTabNode1, kTab1);
  // Regular associations.
  tracker_.ReassociateLocalTab(kTabNode2, kTab2);
  tracker_.ReassociateLocalTab(kTabNode3, kTab3);

  base::MockCallback<base::RepeatingCallback<void(
      const std::string& session_name, sync_pb::SessionSpecifics* specifics)>>
      callback;
  EXPECT_CALL(callback,
              Run(kSessionName, Pointee(MatchesHeader(
                                    kTag, kSessionStartTime, {kWindow1.id()},
                                    {kTab1.id(), kTab2.id()}))));
  EXPECT_CALL(callback, Run(kSessionName,
                            Pointee(MatchesTab(kTag, kWindow1.id(), kTab1.id(),
                                               kTabNode1, /*urls=*/_))));
  EXPECT_CALL(callback, Run(kSessionName,
                            Pointee(MatchesTab(kTag, kWindow1.id(), kTab2.id(),
                                               kTabNode2, /*urls=*/_))));
  EXPECT_CALL(
      callback,
      Run(kSessionName, Pointee(MatchesTab(kTag, Ne(kWindow1.id()), kTab3.id(),
                                           kTabNode3, /*urls=*/_))));

  SerializeTrackerToSpecifics(tracker_, callback.Get());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&callback));

  // Serialize the header only.
  EXPECT_CALL(callback,
              Run(kSessionName, Pointee(MatchesHeader(
                                    kTag, kSessionStartTime, {kWindow1.id()},
                                    {kTab1.id(), kTab2.id()}))));
  SerializePartialTrackerToSpecifics(
      tracker_, {{kTag, {TabNodePool::kInvalidTabNodeID}}}, callback.Get());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&callback));

  // Serialize a known and associated tab.
  EXPECT_CALL(callback, Run(kSessionName,
                            Pointee(MatchesTab(kTag, kWindow1.id(), kTab1.id(),
                                               kTabNode1, /*urls=*/_))));
  SerializePartialTrackerToSpecifics(tracker_, {{kTag, {kTabNode1}}},
                                     callback.Get());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&callback));

  // Attempt to serialize unknown entities.
  EXPECT_CALL(callback, Run).Times(0);
  SerializePartialTrackerToSpecifics(tracker_, {{kTag, {kTabNode5}}},
                                     callback.Get());
  SerializePartialTrackerToSpecifics(
      tracker_, {{kTag2, {TabNodePool::kInvalidTabNodeID, kTabNode1}}},
      callback.Get());
}

TEST_F(SyncedSessionTrackerTest, SerializeTrackerToSpecificsWithEmptyHeader) {
  sync_pb::SessionSpecifics tab;
  tab.set_session_tag(kTag);
  tab.set_tab_node_id(kTabNode1);
  tab.mutable_tab()->set_window_id(kWindow1.id());
  tab.mutable_tab()->set_tab_id(kTab1.id());
  UpdateTrackerWithSpecifics(tab, base::Time::Now(), &tracker_);

  sync_pb::SessionSpecifics header;
  header.set_session_tag(kTag);
  header.mutable_header()->set_client_name(kSessionName);
  UpdateTrackerWithSpecifics(header, base::Time::Now(), &tracker_);

  base::MockCallback<base::RepeatingCallback<void(
      const std::string& session_name, sync_pb::SessionSpecifics* specifics)>>
      callback;
  EXPECT_CALL(callback,
              Run(kSessionName, Pointee(MatchesHeader(kTag, /*window_ids=*/{},
                                                      /*tab_ids=*/{}))));
  EXPECT_CALL(
      callback,
      Run(kSessionName, Pointee(MatchesTab(kTag, /*window_id=*/0, kTab1.id(),
                                           kTabNode1, /*urls=*/_))));
  SerializeTrackerToSpecifics(tracker_, callback.Get());
}

}  // namespace sync_sessions
