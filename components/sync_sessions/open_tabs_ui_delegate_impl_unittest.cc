// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/open_tabs_ui_delegate_impl.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/synced_session_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {
namespace {

using sessions::SessionTab;
using testing::ElementsAre;
using testing::Field;
using testing::Pointee;
using testing::Property;

const char kSessionTag1[] = "foreign1";
const char kSessionTag2[] = "foreign2";
const char kSessionTag3[] = "foreign3";
const SessionID kWindowId1 = SessionID::FromSerializedValue(1);
const SessionID kWindowId2 = SessionID::FromSerializedValue(2);
const SessionID kWindowId3 = SessionID::FromSerializedValue(3);
const SessionID kTabId1 = SessionID::FromSerializedValue(111);
const SessionID kTabId2 = SessionID::FromSerializedValue(222);
const SessionID kTabId3 = SessionID::FromSerializedValue(333);

void IngnoreForeignSessionDeletion(const std::string& session_tag) {}

class OpenTabsUIDelegateImplTest : public testing::Test {
 protected:
  OpenTabsUIDelegateImplTest()
      : session_tracker_(&mock_sync_sessions_client_),
        delegate_(&mock_sync_sessions_client_,
                  &session_tracker_,
                  base::BindRepeating(&IngnoreForeignSessionDeletion)) {}

  testing::NiceMock<MockSyncSessionsClient> mock_sync_sessions_client_;
  SyncedSessionTracker session_tracker_;
  OpenTabsUIDelegateImpl delegate_;
};

TEST_F(OpenTabsUIDelegateImplTest, ShouldSortSessions) {
  const base::Time kTime0 = base::Time::Now();

  // Create three sessions, with one window and tab each.
  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId1);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId1, kTabId1);
  sessions::SerializedNavigationEntry entry1 =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  entry1.set_virtual_url(GURL("http://url1"));
  entry1.set_title(u"title1");
  session_tracker_.GetTab(kSessionTag1, kTabId1)->navigations.push_back(entry1);
  session_tracker_.GetSession(kSessionTag1)
      ->SetModifiedTime(kTime0 + base::Seconds(3));

  session_tracker_.PutWindowInSession(kSessionTag2, kWindowId2);
  session_tracker_.PutTabInWindow(kSessionTag2, kWindowId2, kTabId2);
  sessions::SerializedNavigationEntry entry2 =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  entry2.set_virtual_url(GURL("http://url2"));
  entry2.set_title(u"title2");
  session_tracker_.GetTab(kSessionTag2, kTabId2)->navigations.push_back(entry2);
  session_tracker_.GetSession(kSessionTag2)
      ->SetModifiedTime(kTime0 + base::Seconds(1));

  session_tracker_.PutWindowInSession(kSessionTag3, kWindowId3);
  session_tracker_.PutTabInWindow(kSessionTag3, kWindowId3, kTabId3);
  sessions::SerializedNavigationEntry entry3 =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  entry3.set_virtual_url(GURL("http://url3"));
  entry3.set_title(u"title3");
  session_tracker_.GetTab(kSessionTag3, kTabId3)->navigations.push_back(entry3);
  session_tracker_.GetSession(kSessionTag3)
      ->SetModifiedTime(kTime0 + base::Seconds(2));

  std::vector<raw_ptr<const SyncedSession, VectorExperimental>> sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(&sessions));
  EXPECT_EQ(sessions[0]->GetSessionTag(), kSessionTag1);
  EXPECT_EQ(sessions[1]->GetSessionTag(), kSessionTag3);
  EXPECT_EQ(sessions[2]->GetSessionTag(), kSessionTag2);
}

TEST_F(OpenTabsUIDelegateImplTest, ShouldSortTabs) {
  const base::Time kTime0 = base::Time::Now();
  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId1);
  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId2);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId1, kTabId1);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId2, kTabId2);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId2, kTabId3);

  sessions::SessionTab* tab1 = session_tracker_.GetTab(kSessionTag1, kTabId1);
  tab1->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest());
  tab1->timestamp = kTime0 + base::Seconds(3);

  sessions::SessionTab* tab2 = session_tracker_.GetTab(kSessionTag1, kTabId2);
  tab2->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest());
  tab2->timestamp = kTime0 + base::Seconds(1);

  sessions::SessionTab* tab3 = session_tracker_.GetTab(kSessionTag1, kTabId3);
  tab3->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest());
  tab3->timestamp = kTime0 + base::Seconds(2);

  std::vector<const SessionTab*> tabs;
  EXPECT_TRUE(delegate_.GetForeignSessionTabs(kSessionTag1, &tabs));
  EXPECT_THAT(
      tabs,
      ElementsAre(Pointee(Field(&SessionTab::tab_id,
                                Property(&SessionID::id, kTabId1.id()))),
                  Pointee(Field(&SessionTab::tab_id,
                                Property(&SessionID::id, kTabId3.id()))),
                  Pointee(Field(&SessionTab::tab_id,
                                Property(&SessionID::id, kTabId2.id())))));
}

TEST_F(OpenTabsUIDelegateImplTest, ShouldSkipNonPresentable) {
  // Create two sessions, with one window and tab each, but only the second
  // contains a navigation.
  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId1);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId1, kTabId1);
  session_tracker_.GetTab(kSessionTag1, kTabId1);

  session_tracker_.PutWindowInSession(kSessionTag2, kWindowId2);
  session_tracker_.PutTabInWindow(kSessionTag2, kWindowId2, kTabId2);
  session_tracker_.GetTab(kSessionTag2, kTabId2)
      ->navigations.push_back(sessions::SerializedNavigationEntryTestHelper::
                                  CreateNavigationForTest());

  std::vector<raw_ptr<const SyncedSession, VectorExperimental>> sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(&sessions));
  EXPECT_EQ(sessions[0]->GetSessionTag(), kSessionTag2);
}

TEST_F(OpenTabsUIDelegateImplTest, ShouldSkipNonSyncableTabs) {
  ON_CALL(mock_sync_sessions_client_, ShouldSyncURL(GURL("http://url1")))
      .WillByDefault(testing::Return(false));

  // Create two sessions, with one window and tab each. The first of the two
  // contains a URL that should not be synced.
  sessions::SerializedNavigationEntry nonsyncable_entry =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  nonsyncable_entry.set_virtual_url(GURL("http://url1"));
  nonsyncable_entry.set_title(u"title1");
  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId1);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId1, kTabId1);
  session_tracker_.GetTab(kSessionTag1, kTabId1)
      ->navigations.push_back(nonsyncable_entry);

  sessions::SerializedNavigationEntry syncable_entry =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  syncable_entry.set_virtual_url(GURL("http://otherurl"));
  syncable_entry.set_title(u"title1");
  session_tracker_.PutWindowInSession(kSessionTag2, kWindowId2);
  session_tracker_.PutTabInWindow(kSessionTag2, kWindowId2, kTabId2);
  session_tracker_.GetTab(kSessionTag2, kTabId2)
      ->navigations.push_back(syncable_entry);

  std::vector<raw_ptr<const SyncedSession, VectorExperimental>> sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(&sessions));
  EXPECT_EQ(sessions[0]->GetSessionTag(), kSessionTag2);
}

}  // namespace
}  // namespace sync_sessions
