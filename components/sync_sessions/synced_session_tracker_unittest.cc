// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session_tracker.h"

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "components/sync_sessions/test_matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::IsNull;
using testing::Ne;
using testing::NotNull;
using testing::Pointee;
using testing::_;

namespace sync_sessions {

namespace {

const char kValidUrl[] = "http://www.example.com";
const char kSessionName[] = "sessionname";
const sync_pb::SyncEnums::DeviceType kDeviceType =
    sync_pb::SyncEnums_DeviceType_TYPE_PHONE;
const char kTag[] = "tag";
const char kTag2[] = "tag2";
const char kTag3[] = "tag3";
const char kTitle[] = "title";
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
  return arg->session_tag == expected_tag;
}

}  // namespace

class SyncedSessionTrackerTest : public testing::Test {
 public:
  SyncedSessionTrackerTest() : tracker_(&sessions_client_) {}
  ~SyncedSessionTrackerTest() override {}

  SyncedSessionTracker* GetTracker() { return &tracker_; }
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
    for (auto& window_pair : session->synced_session.windows) {
      mapped_tab_count += window_pair.second->wrapped_window.tabs.size();
      for (auto& tab : window_pair.second->wrapped_window.tabs) {
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
    for (const auto& tab_pair : session->unmapped_tabs) {
      if (tab_pair.first != tab_pair.second->tab_id) {
        return AssertionFailure()
               << "Unmapped tab " << tab_pair.second->tab_id.id()
               << " associated with wrong tab " << tab_pair.first;
      }
      const auto tab_map_it =
          session->synced_tab_map.find(tab_pair.second->tab_id);
      if (tab_map_it == session->synced_tab_map.end()) {
        return AssertionFailure() << "Unmapped tab " << tab_pair.second->tab_id
                                  << " has no corresponding synced tab entry";
      }
      if (tab_map_it->second != tab_pair.second.get()) {
        return AssertionFailure()
               << "Unmapped tab " << tab_pair.second->tab_id.id()
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

  MockSyncSessionsClient* GetSyncSessionsClient() { return &sessions_client_; }

 private:
  testing::NiceMock<MockSyncSessionsClient> sessions_client_;
  SyncedSessionTracker tracker_;
};

TEST_F(SyncedSessionTrackerTest, GetSession) {
  SyncedSession* session1 = GetTracker()->GetSession(kTag);
  SyncedSession* session2 = GetTracker()->GetSession(kTag2);
  ASSERT_EQ(session1, GetTracker()->LookupSession(kTag));
  ASSERT_EQ(session1, GetTracker()->GetSession(kTag));
  ASSERT_NE(session1, session2);
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, GetTabUnmapped) {
  sessions::SessionTab* tab = GetTracker()->GetTab(kTag, kTab1);
  ASSERT_EQ(tab, GetTracker()->GetTab(kTag, kTab1));
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, PutWindowInSession) {
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  const SyncedSession* session = GetTracker()->LookupSession(kTag);
  ASSERT_EQ(1U, session->windows.size());

  // Doing it again should have no effect.
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  ASSERT_EQ(1U, session->windows.size());
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, PutTabInWindow) {
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  const SyncedSession* session = GetTracker()->LookupSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows.at(kWindow1)->wrapped_window.tabs.size());
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, LookupAllSessions) {
  EXPECT_THAT(
      GetTracker()->LookupAllSessions(SyncedSessionTracker::PRESENTABLE),
      IsEmpty());

  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);

  EXPECT_THAT(GetTracker()->LookupAllSessions(SyncedSessionTracker::RAW),
              ElementsAre(HasSessionTag(kTag)));
  EXPECT_THAT(
      GetTracker()->LookupAllSessions(SyncedSessionTracker::PRESENTABLE),
      IsEmpty());

  sessions::SessionTab* tab = GetTracker()->GetTab(kTag, kTab1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(kValidUrl,
                                                                      kTitle));
  EXPECT_THAT(
      GetTracker()->LookupAllSessions(SyncedSessionTracker::PRESENTABLE),
      ElementsAre(HasSessionTag(kTag)));

  GetTracker()->GetSession(kTag2);
  GetTracker()->PutWindowInSession(kTag2, kWindow1);
  GetTracker()->PutTabInWindow(kTag2, kWindow1, kTab2);

  sessions::SessionTab* tab2 = GetTracker()->GetTab(kTag2, kTab2);
  ASSERT_TRUE(tab2);
  tab2->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(kValidUrl,
                                                                      kTitle));
  EXPECT_THAT(
      GetTracker()->LookupAllSessions(SyncedSessionTracker::PRESENTABLE),
      ElementsAre(HasSessionTag(kTag), HasSessionTag(kTag2)));
}

TEST_F(SyncedSessionTrackerTest, LookupAllForeignSessions) {
  const char kInvalidUrl[] = "invalid.url";
  ON_CALL(*GetSyncSessionsClient(), ShouldSyncURL(GURL(kInvalidUrl)))
      .WillByDefault(testing::Return(false));

  EXPECT_THAT(
      GetTracker()->LookupAllForeignSessions(SyncedSessionTracker::PRESENTABLE),
      IsEmpty());
  GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  sessions::SessionTab* tab = GetTracker()->GetTab(kTag, kTab1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(kValidUrl,
                                                                      kTitle));
  GetTracker()->GetSession(kTag2);
  GetTracker()->GetSession(kTag3);
  GetTracker()->PutWindowInSession(kTag3, kWindow1);
  GetTracker()->PutTabInWindow(kTag3, kWindow1, kTab1);
  tab = GetTracker()->GetTab(kTag3, kTab1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kInvalidUrl, kTitle));
  // Only the session with a valid window and tab gets returned.
  EXPECT_THAT(
      GetTracker()->LookupAllForeignSessions(SyncedSessionTracker::PRESENTABLE),
      ElementsAre(HasSessionTag(kTag)));
  EXPECT_THAT(GetTracker()->LookupAllForeignSessions(SyncedSessionTracker::RAW),
              ElementsAre(HasSessionTag(kTag), HasSessionTag(kTag2),
                          HasSessionTag(kTag3)));
}

TEST_F(SyncedSessionTrackerTest, LookupSessionWindows) {
  std::vector<const sessions::SessionWindow*> windows;
  ASSERT_FALSE(GetTracker()->LookupSessionWindows(kTag, &windows));
  GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutWindowInSession(kTag, kWindow2);
  GetTracker()->GetSession(kTag2);
  GetTracker()->PutWindowInSession(kTag2, kWindow1);
  GetTracker()->PutWindowInSession(kTag2, kWindow2);
  ASSERT_TRUE(GetTracker()->LookupSessionWindows(kTag, &windows));
  ASSERT_EQ(2U, windows.size());  // Only windows from kTag session.
  ASSERT_NE((sessions::SessionWindow*)nullptr, windows[0]);
  ASSERT_NE((sessions::SessionWindow*)nullptr, windows[1]);
  ASSERT_NE(windows[1], windows[0]);
}

TEST_F(SyncedSessionTrackerTest, LookupSessionTab) {
  ASSERT_THAT(GetTracker()->LookupSessionTab(kTag, SessionID::InvalidValue()),
              IsNull());
  ASSERT_THAT(GetTracker()->LookupSessionTab(kTag, kTab1), IsNull());
  GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  ASSERT_THAT(GetTracker()->LookupSessionTab(kTag, kTab1), NotNull());
}

TEST_F(SyncedSessionTrackerTest, Complex) {
  std::vector<sessions::SessionTab *> tabs1, tabs2;
  sessions::SessionTab* temp_tab;
  ASSERT_TRUE(GetTracker()->Empty());
  ASSERT_EQ(0U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(0U, GetTracker()->num_synced_tabs(kTag));
  tabs1.push_back(GetTracker()->GetTab(kTag, kTab1));
  tabs1.push_back(GetTracker()->GetTab(kTag, kTab2));
  tabs1.push_back(GetTracker()->GetTab(kTag, kTab3));
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(1U, GetTracker()->num_synced_sessions());
  temp_tab = GetTracker()->GetTab(kTag, kTab1);  // Already created.
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(1U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(tabs1[0], temp_tab);
  tabs2.push_back(GetTracker()->GetTab(kTag2, kTab1));
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(kTag2));
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());
  ASSERT_FALSE(GetTracker()->DeleteForeignSession(kTag3));

  SyncedSession* session = GetTracker()->GetSession(kTag);
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());
  SyncedSession* session2 = GetTracker()->GetSession(kTag2);
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());
  SyncedSession* session3 = GetTracker()->GetSession(kTag3);
  session3->device_type = sync_pb::SyncEnums_DeviceType_TYPE_LINUX;
  ASSERT_EQ(3U, GetTracker()->num_synced_sessions());

  ASSERT_TRUE(session);
  ASSERT_TRUE(session2);
  ASSERT_TRUE(session3);
  ASSERT_NE(session, session2);
  ASSERT_NE(session2, session3);
  ASSERT_TRUE(GetTracker()->DeleteForeignSession(kTag3));
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());

  GetTracker()->PutWindowInSession(kTag, kWindow1);     // Create a window.
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab3);  // No longer unmapped.
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(kTag));  // Has not changed.

  ASSERT_EQ(tabs1[0], GetTracker()->LookupSessionTab(kTag, kTab1));
  ASSERT_EQ(tabs1[2], GetTracker()->LookupSessionTab(kTag, kTab3));
  ASSERT_THAT(GetTracker()->LookupSessionTab(kTag, kTab4), IsNull());

  std::vector<const sessions::SessionWindow*> windows;
  ASSERT_TRUE(GetTracker()->LookupSessionWindows(kTag, &windows));
  ASSERT_EQ(1U, windows.size());
  ASSERT_TRUE(GetTracker()->LookupSessionWindows(kTag2, &windows));
  ASSERT_EQ(0U, windows.size());

  // The sessions don't have valid tabs, lookup should not succeed.
  std::vector<const SyncedSession*> sessions;
  EXPECT_THAT(
      GetTracker()->LookupAllForeignSessions(SyncedSessionTracker::PRESENTABLE),
      IsEmpty());
  EXPECT_THAT(GetTracker()->LookupAllForeignSessions(SyncedSessionTracker::RAW),
              ElementsAre(HasSessionTag(kTag), HasSessionTag(kTag2)));

  GetTracker()->Clear();
  ASSERT_EQ(0U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(0U, GetTracker()->num_synced_tabs(kTag2));
  ASSERT_EQ(0U, GetTracker()->num_synced_sessions());
}

TEST_F(SyncedSessionTrackerTest, ManyGetTabs) {
  ASSERT_TRUE(GetTracker()->Empty());
  const int kMaxSessions = 10;
  const int kMaxTabs = 1000;
  const int kMaxAttempts = 10000;
  for (int j = 0; j < kMaxSessions; ++j) {
    std::string tag = base::StringPrintf("tag%d", j);
    for (int i = 0; i < kMaxAttempts; ++i) {
      // More attempts than tabs means we'll sometimes get the same tabs,
      // sometimes have to allocate new tabs.
      int rand_tab_num = base::RandInt(0, kMaxTabs);
      sessions::SessionTab* tab = GetTracker()->GetTab(
          tag, SessionID::FromSerializedValue(rand_tab_num + 1));
      ASSERT_TRUE(tab);
    }
  }
}

TEST_F(SyncedSessionTrackerTest, LookupTabNodeIds) {
  GetTracker()->OnTabNodeSeen(kTag, 1, kTab1);
  GetTracker()->OnTabNodeSeen(kTag, 2, kTab2);
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag), ElementsAre(1, 2));
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag2), IsEmpty());

  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag), ElementsAre(1, 2));

  GetTracker()->OnTabNodeSeen(kTag, 3, kTab3);
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag), ElementsAre(1, 2, 3));

  GetTracker()->OnTabNodeSeen(kTag2, 21, kTab4);
  GetTracker()->OnTabNodeSeen(kTag2, 22, kTab5);
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag2), ElementsAre(21, 22));
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag), ElementsAre(1, 2, 3));

  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag3), IsEmpty());
  GetTracker()->PutWindowInSession(kTag3, kWindow2);
  GetTracker()->PutTabInWindow(kTag3, kWindow2, kTab2);
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag3), IsEmpty());
  EXPECT_FALSE(GetTracker()->DeleteForeignSession(kTag3));
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag3), IsEmpty());

  EXPECT_FALSE(GetTracker()->DeleteForeignSession(kTag));
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag), IsEmpty());
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag2), ElementsAre(21, 22));

  GetTracker()->OnTabNodeSeen(kTag2, 21, kTab6);
  GetTracker()->OnTabNodeSeen(kTag2, 23, kTab7);
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag2), ElementsAre(21, 22, 23));

  EXPECT_FALSE(GetTracker()->DeleteForeignSession(kTag2));
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag2), IsEmpty());
}

TEST_F(SyncedSessionTrackerTest, LookupUnmappedTabs) {
  EXPECT_THAT(GetTracker()->LookupUnmappedTabs(kTag), IsEmpty());

  sessions::SessionTab* tab = GetTracker()->GetTab(kTag, kTab1);
  ASSERT_THAT(tab, NotNull());

  EXPECT_THAT(GetTracker()->LookupUnmappedTabs(kTag), ElementsAre(tab));
  EXPECT_THAT(GetTracker()->LookupUnmappedTabs(kTag2), IsEmpty());

  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  EXPECT_THAT(GetTracker()->LookupUnmappedTabs(kTag), IsEmpty());
}

TEST_F(SyncedSessionTrackerTest, SessionTracking) {
  ASSERT_TRUE(GetTracker()->Empty());

  // Create some session information that is stale.
  SyncedSession* session1 = GetTracker()->GetSession(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->GetTab(kTag, kTab3)->window_id =
      SessionID::FromSerializedValue(1);  // Unmapped.
  GetTracker()->GetTab(kTag, kTab4)->window_id =
      SessionID::FromSerializedValue(1);  // Unmapped.
  GetTracker()->PutWindowInSession(kTag, kWindow2);
  GetTracker()->PutTabInWindow(kTag, kWindow2, kTab5);
  GetTracker()->PutTabInWindow(kTag, kWindow2, kTab6);
  ASSERT_EQ(2U, session1->windows.size());
  ASSERT_EQ(2U, session1->windows[kWindow1]->wrapped_window.tabs.size());
  ASSERT_EQ(2U, session1->windows[kWindow2]->wrapped_window.tabs.size());
  ASSERT_EQ(6U, GetTracker()->num_synced_tabs(kTag));

  // Create a session that should not be affected.
  SyncedSession* session2 = GetTracker()->GetSession(kTag2);
  GetTracker()->PutWindowInSession(kTag2, kWindow3);
  GetTracker()->PutTabInWindow(kTag2, kWindow3, kTab2);
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[kWindow3]->wrapped_window.tabs.size());
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(kTag2));

  // Reset tracking and get the current windows/tabs.
  // We simulate moving a tab from one window to another, then closing the
  // first window (including its one remaining tab), and opening a new tab
  // on the remaining window.

  // New tab, arrived before meta node so unmapped.
  GetTracker()->GetTab(kTag, kTab7);
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  // Tab 1 is closed.
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab3);  // No longer unmapped.
  // Tab 3 was unmapped and does not get used.
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab5);  // Moved from window 1.
  // Window 1 was closed, along with tab 5.
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab7);  // No longer unmapped.
  // Session 2 should not be affected.
  GetTracker()->CleanupSession(kTag);

  // Verify that only those parts of the session not owned have been removed.
  ASSERT_EQ(1U, session1->windows.size());
  ASSERT_EQ(4U, session1->windows[kWindow1]->wrapped_window.tabs.size());
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[kWindow3]->wrapped_window.tabs.size());
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(4U, GetTracker()->num_synced_tabs(kTag));
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(kTag2));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));

  // All memory should be properly deallocated by destructor for the
  // SyncedSessionTracker.
}

TEST_F(SyncedSessionTrackerTest, DeleteForeignTab) {
  int tab_node_id_1 = 1;
  int tab_node_id_2 = 2;
  std::set<int> result;

  GetTracker()->OnTabNodeSeen(kTag, tab_node_id_1, kTab1);
  GetTracker()->OnTabNodeSeen(kTag, tab_node_id_2, kTab2);

  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag),
              ElementsAre(tab_node_id_1, tab_node_id_2));

  GetTracker()->DeleteForeignTab(kTag, tab_node_id_1);
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag), ElementsAre(tab_node_id_2));

  GetTracker()->DeleteForeignTab(kTag, tab_node_id_2);
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag), IsEmpty());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, CleanupLocalTabs) {
  std::set<int> free_node_ids;

  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);

  // Start with two restored tab nodes.
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  GetTracker()->ReassociateLocalTab(kTabNode2, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  EXPECT_TRUE(free_node_ids.empty());

  // Associate with no tabs. The tab pool should now be full.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  EXPECT_TRUE(free_node_ids.empty());

  // Associate with only 1 tab open. A tab node should be reused.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  EXPECT_EQ(kTabNode1, GetTracker()->AssociateLocalTabWithFreeTabNode(kTab1));
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  EXPECT_TRUE(free_node_ids.empty());

  // Simulate a tab opening, which should use the last free tab node.
  EXPECT_EQ(kTabNode2, GetTracker()->AssociateLocalTabWithFreeTabNode(kTab2));
  EXPECT_EQ(kTabNode2, GetTracker()->LookupTabNodeFromTabId(kTag, kTab2));

  // Simulate another tab opening, which should create a new associated tab
  // node.
  EXPECT_EQ(kTabNode3, GetTracker()->AssociateLocalTabWithFreeTabNode(kTab3));
  EXPECT_EQ(kTabNode3, GetTracker()->LookupTabNodeFromTabId(kTag, kTab3));

  // Previous tabs should still be associated.
  EXPECT_EQ(kTabNode1, GetTracker()->LookupTabNodeFromTabId(kTag, kTab1));
  EXPECT_EQ(kTabNode2, GetTracker()->LookupTabNodeFromTabId(kTag, kTab2));

  // Associate with no tabs. All tabs should be freed again, and the pool
  // should now be full.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  EXPECT_TRUE(free_node_ids.empty());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabMapped) {
  std::set<int> free_node_ids;

  // First create the tab normally.
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map it to a window with the same tab id as it was created with.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  const SyncedSession* session = GetTracker()->LookupSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows.at(kWindow1)->wrapped_window.tabs.size());
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());

  // Then reassociate with a new tab id.
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Reset tracking, and put the new tab id into the window.
  GetTracker()->ResetSessionTracking(kTag);
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());
  ASSERT_EQ(GetTracker()->LookupTabNodeIds(kTag).size(),
            GetTracker()->LookupTabNodeIds(kTag).count(kTabNode1));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabMappedTwice) {
  std::set<int> free_node_ids;

  // First create the tab normally.
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map it to a window with the same tab id as it was created with.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  const SyncedSession* session = GetTracker()->LookupSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows.at(kWindow1)->wrapped_window.tabs.size());
  EXPECT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());

  // Then reassociate with a new tab id.
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Tab 1 should no longer be associated with any SessionTab object. At this
  // point there's no need to verify it's unmapped state.
  EXPECT_THAT(GetTracker()->LookupSessionTab(kTag, kTab1), IsNull());

  // Reset tracking and add back both the old tab and the new tab (both of which
  // refer to the same tab node id).
  GetTracker()->ResetSessionTracking(kTag);
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  EXPECT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows.at(kWindow1)->wrapped_window.tabs[1].get());
  EXPECT_EQ(GetTracker()->LookupTabNodeIds(kTag).size(),
            GetTracker()->LookupTabNodeIds(kTag).count(kTabNode1));

  // Attempting to access the original tab will create a new SessionTab object.
  EXPECT_NE(GetTracker()->GetTab(kTag, kTab1),
            GetTracker()->GetTab(kTag, kTab2));
  EXPECT_EQ(TabNodePool::kInvalidTabNodeID,
            GetTracker()->LookupTabNodeFromTabId(kTag, kTab1));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabUnmapped) {
  std::set<int> free_node_ids;

  // First create the old tab in an unmapped state.
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map it to a window, but reassociated with a new tab id.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as GetTab.
  const SyncedSession* session = GetTracker()->LookupSession(kTag);
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());
  ASSERT_EQ(GetTracker()->LookupTabNodeIds(kTag).size(),
            GetTracker()->LookupTabNodeIds(kTag).count(kTabNode1));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabOldUnmappedNewMapped) {
  std::set<int> free_node_ids;

  // First create the old tab in an unmapped state.
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map an unseen tab to a window, then reassociate the existing tab to the
  // mapped tab id.
  GetTracker()->ResetSessionTracking(kTag);
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as GetTab.
  const SyncedSession* session = GetTracker()->LookupSession(kTag);
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());
  ASSERT_EQ(GetTracker()->LookupTabNodeIds(kTag).size(),
            GetTracker()->LookupTabNodeIds(kTag).count(kTabNode1));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabSameTabId) {
  std::set<int> free_node_ids;

  // First create the tab normally.
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Map it to a window.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  const SyncedSession* session = GetTracker()->LookupSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows.at(kWindow1)->wrapped_window.tabs.size());
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());

  // Reassociate, using the same tab id.
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Reset tracking, and put the tab id back into the same window.
  GetTracker()->ResetSessionTracking(kTag);
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(free_node_ids.empty());
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());
  ASSERT_EQ(GetTracker()->LookupTabNodeIds(kTag).size(),
            GetTracker()->LookupTabNodeIds(kTag).count(kTabNode1));
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, ReassociateTabOldMappedNewUnmapped) {
  std::set<int> free_node_ids;

  // First create an unmapped tab.
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode1));
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab1));

  // Now, map the first one, deleting the second one.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  const SyncedSession* session = GetTracker()->LookupSession(kTag);
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows.at(kWindow1)->wrapped_window.tabs.size());
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab1),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());

  // Create a second unmapped tab.
  GetTracker()->ReassociateLocalTab(kTabNode2, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode2));
  EXPECT_TRUE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Reassociate the second tab with node of the first tab.
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab2);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_TRUE(IsLocalTabNodeAssociated(kTabNode1));
  EXPECT_FALSE(IsLocalTabNodeAssociated(kTabNode2));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now map the new one.
  GetTracker()->ResetSessionTracking(kTag);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  GetTracker()->CleanupLocalTabs(&free_node_ids);
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab1));
  EXPECT_FALSE(GetTracker()->IsTabUnmappedForTesting(kTab2));

  // Now that it's been mapped, it should be accessible both via the
  // GetSession as well as the GetTab.
  ASSERT_EQ(GetTracker()->GetTab(kTag, kTab2),
            session->windows.at(kWindow1)->wrapped_window.tabs[0].get());
  ASSERT_TRUE(VerifyTabIntegrity(kTag));
}

TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithHeader) {
  sync_pb::SessionSpecifics header;
  header.set_session_tag(kTag);
  header.mutable_header()->add_window()->set_window_id(kWindow1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab2.id());
  UpdateTrackerWithSpecifics(header, base::Time::Now(), GetTracker());

  EXPECT_THAT(
      GetTracker()->LookupSession(kTag),
      MatchesSyncedSession(kTag, {{kWindow1.id(), {kTab1.id(), kTab2.id()}}}));
  EXPECT_THAT(GetTracker()->LookupSessionTab(kTag, kTab1), NotNull());
  EXPECT_THAT(GetTracker()->LookupSessionTab(kTag, kTab2), NotNull());
}

TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithIdenticalHeader) {
  sync_pb::SessionSpecifics header;
  header.set_session_tag(kTag);
  header.mutable_header()->add_window()->set_window_id(kWindow1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab2.id());
  UpdateTrackerWithSpecifics(header, base::Time::Now(), GetTracker());
  ASSERT_THAT(
      GetTracker()->LookupSession(kTag),
      MatchesSyncedSession(kTag, {{kWindow1.id(), {kTab1.id(), kTab2.id()}}}));

  // Update with an exact header entity.
  UpdateTrackerWithSpecifics(header, base::Time::Now(), GetTracker());

  EXPECT_THAT(
      GetTracker()->LookupSession(kTag),
      MatchesSyncedSession(kTag, {{kWindow1.id(), {kTab1.id(), kTab2.id()}}}));
  EXPECT_THAT(GetTracker()->LookupSessionTab(kTag, kTab1), NotNull());
}

// Verifies that an invalid header (with duplicated tab IDs) is discarded.
TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithHeaderWithDuplicateTabIds) {
  sync_pb::SessionSpecifics header;
  header.set_session_tag(kTag);
  header.mutable_header()->add_window()->set_window_id(kWindow1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab1.id());
  UpdateTrackerWithSpecifics(header, base::Time::Now(), GetTracker());

  EXPECT_THAT(GetTracker()->LookupSession(kTag),
              MatchesSyncedSession(kTag, /*window_id_to_tabs=*/{}));
}

// Verifies that an invalid tab (with invalid ID) is discarded.
TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithInvalidTab) {
  const int kInvalidTabId = -1;
  sync_pb::SessionSpecifics tab;
  tab.set_session_tag(kTag);
  tab.mutable_tab()->set_tab_id(kInvalidTabId);
  UpdateTrackerWithSpecifics(tab, base::Time::Now(), GetTracker());

  EXPECT_THAT(GetTracker()->LookupSessionTab(
                  kTag, SessionID::FromSerializedValue(kInvalidTabId)),
              IsNull());
}

TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithTab) {
  sync_pb::SessionSpecifics tab;
  tab.set_session_tag(kTag);
  tab.set_tab_node_id(kTabNode1);
  tab.mutable_tab()->set_window_id(kWindow1.id());
  tab.mutable_tab()->set_tab_id(kTab1.id());
  UpdateTrackerWithSpecifics(tab, base::Time::Now(), GetTracker());

  const sessions::SessionTab* tracked_tab =
      GetTracker()->LookupSessionTab(kTag, kTab1);
  ASSERT_THAT(tracked_tab, NotNull());
  EXPECT_EQ(kWindow1, tracked_tab->window_id);
  EXPECT_EQ(false, tracked_tab->pinned);

  EXPECT_THAT(GetTracker()->LookupSession(kTag),
              MatchesSyncedSession(kTag, /*window_id_to_tabs*/ {}));

  // Overwrite some attribute.
  tab.mutable_tab()->set_pinned(true);
  UpdateTrackerWithSpecifics(tab, base::Time::Now(), GetTracker());
  tracked_tab = GetTracker()->LookupSessionTab(kTag, kTab1);
  ASSERT_THAT(tracked_tab, NotNull());
  EXPECT_EQ(kWindow1, tracked_tab->window_id);
  EXPECT_EQ(true, tracked_tab->pinned);

  EXPECT_THAT(GetTracker()->LookupSession(kTag),
              MatchesSyncedSession(kTag, /*window_id_to_tabs*/ {}));
}

TEST_F(SyncedSessionTrackerTest, UpdateTrackerWithTabThenHeader) {
  sync_pb::SessionSpecifics tab;
  tab.set_session_tag(kTag);
  tab.set_tab_node_id(kTabNode1);
  tab.mutable_tab()->set_window_id(kWindow1.id());
  tab.mutable_tab()->set_tab_id(kTab1.id());
  UpdateTrackerWithSpecifics(tab, base::Time::Now(), GetTracker());

  EXPECT_THAT(GetTracker()->LookupSession(kTag),
              MatchesSyncedSession(kTag, /*window_id_to_tabs*/ {}));

  sync_pb::SessionSpecifics header;
  header.set_session_tag(kTag);
  header.mutable_header()->add_window()->set_window_id(kWindow1.id());
  header.mutable_header()->mutable_window(0)->add_tab(kTab1.id());
  UpdateTrackerWithSpecifics(header, base::Time::Now(), GetTracker());

  const sessions::SessionTab* tracked_tab =
      GetTracker()->LookupSessionTab(kTag, kTab1);
  ASSERT_THAT(tracked_tab, NotNull());
  EXPECT_EQ(kWindow1, tracked_tab->window_id);

  EXPECT_THAT(GetTracker()->LookupSession(kTag),
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
  UpdateTrackerWithSpecifics(tab1, base::Time::Now(), GetTracker());

  sync_pb::SessionSpecifics tab2;
  tab2.set_session_tag(kTag);
  tab2.set_tab_node_id(kTabNode2);
  tab2.mutable_tab()->set_window_id(kWindow1.id());
  tab2.mutable_tab()->set_tab_id(kTab1.id());
  tab2.mutable_tab()->set_pinned(true);
  UpdateTrackerWithSpecifics(tab2, base::Time::Now(), GetTracker());

  EXPECT_THAT(GetTracker()->LookupSession(kTag),
              MatchesSyncedSession(kTag, /*window_id_to_tabs*/ {}));
  EXPECT_THAT(GetTracker()->LookupTabNodeIds(kTag),
              ElementsAre(kTabNode1, kTabNode2));

  const sessions::SessionTab* tracked_tab =
      GetTracker()->LookupSessionTab(kTag, kTab1);
  ASSERT_THAT(tracked_tab, NotNull());
  EXPECT_EQ(kWindow1, tracked_tab->window_id);
  EXPECT_EQ(true, tracked_tab->pinned);
}

TEST_F(SyncedSessionTrackerTest, SerializeTrackerToSpecifics) {
  GetTracker()->InitLocalSession(kTag, kSessionName, kDeviceType);
  GetTracker()->PutWindowInSession(kTag, kWindow1);
  GetTracker()->GetSession(kTag)->windows[kWindow1]->window_type =
      sync_pb::SessionWindow_BrowserType_TYPE_TABBED;
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab1);
  GetTracker()->PutTabInWindow(kTag, kWindow1, kTab2);
  // Unmapped tab.
  GetTracker()->GetTab(kTag, kTab3);
  // |kTabNode4| will be unassociated, because |kTab1| is associated twice.
  GetTracker()->ReassociateLocalTab(kTabNode4, kTab1);
  GetTracker()->ReassociateLocalTab(kTabNode1, kTab1);
  // Regular associations.
  GetTracker()->ReassociateLocalTab(kTabNode2, kTab2);
  GetTracker()->ReassociateLocalTab(kTabNode3, kTab3);

  base::MockCallback<base::RepeatingCallback<void(
      const std::string& session_name, sync_pb::SessionSpecifics* specifics)>>
      callback;
  EXPECT_CALL(callback, Run(kSessionName,
                            Pointee(MatchesHeader(kTag, {kWindow1.id()},
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

  SerializeTrackerToSpecifics(*GetTracker(), callback.Get());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&callback));

  // Serialize the header only.
  EXPECT_CALL(callback, Run(kSessionName,
                            Pointee(MatchesHeader(kTag, {kWindow1.id()},
                                                  {kTab1.id(), kTab2.id()}))));
  SerializePartialTrackerToSpecifics(*GetTracker(),
                                     {{kTag, {TabNodePool::kInvalidTabNodeID}}},
                                     callback.Get());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&callback));

  // Serialize a known and associated tab.
  EXPECT_CALL(callback, Run(kSessionName,
                            Pointee(MatchesTab(kTag, kWindow1.id(), kTab1.id(),
                                               kTabNode1, /*urls=*/_))));
  SerializePartialTrackerToSpecifics(*GetTracker(), {{kTag, {kTabNode1}}},
                                     callback.Get());
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(&callback));

  // Attempt to serialize unknown entities.
  EXPECT_CALL(callback, Run(_, _)).Times(0);
  SerializePartialTrackerToSpecifics(*GetTracker(), {{kTag, {kTabNode5}}},
                                     callback.Get());
  SerializePartialTrackerToSpecifics(
      *GetTracker(), {{kTag2, {TabNodePool::kInvalidTabNodeID, kTabNode1}}},
      callback.Get());
}

TEST_F(SyncedSessionTrackerTest, SerializeTrackerToSpecificsWithEmptyHeader) {
  sync_pb::SessionSpecifics tab;
  tab.set_session_tag(kTag);
  tab.set_tab_node_id(kTabNode1);
  tab.mutable_tab()->set_window_id(kWindow1.id());
  tab.mutable_tab()->set_tab_id(kTab1.id());
  UpdateTrackerWithSpecifics(tab, base::Time::Now(), GetTracker());

  sync_pb::SessionSpecifics header;
  header.set_session_tag(kTag);
  header.mutable_header()->set_client_name(kSessionName);
  UpdateTrackerWithSpecifics(header, base::Time::Now(), GetTracker());

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
  SerializeTrackerToSpecifics(*GetTracker(), callback.Get());
}

}  // namespace sync_sessions
