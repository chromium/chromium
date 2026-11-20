// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/open_tabs_ui_delegate_impl.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync/base/features.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/sync_sessions/synced_session_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {
namespace {

using sessions::SessionTab;
using testing::ElementsAre;
using testing::Field;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Pointee;
using testing::Property;
using testing::Return;
using testing::Test;

const char kSessionTag1[] = "foreign1";
const char kSessionTag2[] = "foreign2";
const char kSessionTag3[] = "foreign3";
constexpr char kUrl1[] = "http://url1";
const SessionID kWindowId1 = SessionID::FromSerializedValue(1);
const SessionID kWindowId2 = SessionID::FromSerializedValue(2);
const SessionID kWindowId3 = SessionID::FromSerializedValue(3);
const SessionID kTabId1 = SessionID::FromSerializedValue(111);
const SessionID kTabId2 = SessionID::FromSerializedValue(222);
const SessionID kTabId3 = SessionID::FromSerializedValue(333);

void IgnoreForeignSessionDeletion(const std::string& session_tag) {}

MATCHER_P(HasSessionTag, expected_tag, "") {
  return arg->GetSessionTag() == expected_tag;
}

class OpenTabsUIDelegateImplTest : public Test {
 protected:
  OpenTabsUIDelegateImplTest()
      : session_sync_prefs_(RegisterAndReturnPrefs(&pref_service_)),
        session_tracker_(&mock_sync_sessions_client_),
        delegate_(&mock_sync_sessions_client_,
                  &session_tracker_,
                  base::BindRepeating(&IgnoreForeignSessionDeletion)) {
    ON_CALL(mock_sync_sessions_client_, GetSessionSyncPrefs())
        .WillByDefault(Return(&session_sync_prefs_));
  }

  static PrefService* RegisterAndReturnPrefs(
      TestingPrefServiceSimple* pref_service) {
    SessionSyncPrefs::RegisterProfilePrefs(pref_service->registry());
    return pref_service;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kDeviceTabVisibilitySettings};
#endif
  TestingPrefServiceSimple pref_service_;
  SessionSyncPrefs session_sync_prefs_;
  NiceMock<MockSyncSessionsClient> mock_sync_sessions_client_;
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
      ->SetModifiedTime(kTime0 - base::Seconds(3));

  session_tracker_.PutWindowInSession(kSessionTag2, kWindowId2);
  session_tracker_.PutTabInWindow(kSessionTag2, kWindowId2, kTabId2);
  sessions::SerializedNavigationEntry entry2 =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  entry2.set_virtual_url(GURL("http://url2"));
  entry2.set_title(u"title2");
  session_tracker_.GetTab(kSessionTag2, kTabId2)->navigations.push_back(entry2);
  session_tracker_.GetSession(kSessionTag2)
      ->SetModifiedTime(kTime0 - base::Seconds(1));

  session_tracker_.PutWindowInSession(kSessionTag3, kWindowId3);
  session_tracker_.PutTabInWindow(kSessionTag3, kWindowId3, kTabId3);
  sessions::SerializedNavigationEntry entry3 =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  entry3.set_virtual_url(GURL("http://url3"));
  entry3.set_title(u"title3");
  session_tracker_.GetTab(kSessionTag3, kTabId3)->navigations.push_back(entry3);
  session_tracker_.GetSession(kSessionTag3)
      ->SetModifiedTime(kTime0 - base::Seconds(2));

  std::vector<raw_ptr<const SyncedSession, DanglingUntriaged>> sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(
      &sessions, OpenTabsUIDelegate::ForeignSessionFilter::kPresentable));
  EXPECT_EQ(sessions[0]->GetSessionTag(), kSessionTag2);
  EXPECT_EQ(sessions[1]->GetSessionTag(), kSessionTag3);
  EXPECT_EQ(sessions[2]->GetSessionTag(), kSessionTag1);
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

  std::vector<raw_ptr<const SyncedSession, DanglingUntriaged>> sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(
      &sessions, OpenTabsUIDelegate::ForeignSessionFilter::kPresentable));
  EXPECT_EQ(sessions[0]->GetSessionTag(), kSessionTag2);
}

TEST_F(OpenTabsUIDelegateImplTest, GetAllForeignSessionLastModifiedTimes) {
  const base::Time kTime0 = base::Time::Now();

  // Create two sessions, with one window and tab each.
  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId1);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId1, kTabId1);
  sessions::SerializedNavigationEntry entry1 =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  entry1.set_virtual_url(GURL("http://url1"));
  entry1.set_title(u"title1");
  session_tracker_.GetTab(kSessionTag1, kTabId1)->navigations.push_back(entry1);
  session_tracker_.GetSession(kSessionTag1)
      ->SetModifiedTime(kTime0 - base::Seconds(3));

  session_tracker_.PutWindowInSession(kSessionTag2, kWindowId2);
  session_tracker_.PutTabInWindow(kSessionTag2, kWindowId2, kTabId2);
  sessions::SerializedNavigationEntry entry2 =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  entry2.set_virtual_url(GURL("http://url2"));
  entry2.set_title(u"title2");
  session_tracker_.GetTab(kSessionTag2, kTabId2)->navigations.push_back(entry2);
  session_tracker_.GetSession(kSessionTag2)
      ->SetModifiedTime(kTime0 - base::Seconds(1));

  // Create a third session that is not presentable (no navigations).
  session_tracker_.PutWindowInSession(kSessionTag3, kWindowId3);
  session_tracker_.PutTabInWindow(kSessionTag3, kWindowId3, kTabId3);
  session_tracker_.GetTab(kSessionTag3, kTabId3);
  session_tracker_.GetSession(kSessionTag3)
      ->SetModifiedTime(kTime0 - base::Seconds(2));

  base::flat_map<std::string, base::Time> timestamps =
      delegate_.GetAllForeignSessionLastModifiedTimes();
  EXPECT_EQ(timestamps.size(), 2u);
  EXPECT_EQ(timestamps[kSessionTag1], kTime0 - base::Seconds(3));
  EXPECT_EQ(timestamps[kSessionTag2], kTime0 - base::Seconds(1));
  EXPECT_FALSE(timestamps.contains(kSessionTag3));
}

TEST_F(OpenTabsUIDelegateImplTest, ShouldSkipNonSyncableTabs) {
  ON_CALL(mock_sync_sessions_client_, ShouldSyncURL(GURL(kUrl1)))
      .WillByDefault(Return(false));

  // Create two sessions, with one window and tab each. The first of the two
  // contains a URL that should not be synced.
  sessions::SerializedNavigationEntry nonsyncable_entry =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  nonsyncable_entry.set_virtual_url(GURL(kUrl1));
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

  std::vector<raw_ptr<const SyncedSession, DanglingUntriaged>> sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(
      &sessions, OpenTabsUIDelegate::ForeignSessionFilter::kPresentable));
  EXPECT_EQ(sessions[0]->GetSessionTag(), kSessionTag2);
}

// Verifies that `GetAllForeignSessions()` with `ForeignSessionFilter::kAll`
// returns named foreign sessions with zero tabs or only non-syncable tabs,
// ordered from most recent to least recent.
TEST_F(OpenTabsUIDelegateImplTest,
       GetAllForeignSessionsWithFilterAllReturnsZeroTabAndNonSyncableSessions) {
  std::vector<raw_ptr<const SyncedSession, DanglingUntriaged>> empty_sessions;
  EXPECT_FALSE(delegate_.GetAllForeignSessions(
      &empty_sessions, OpenTabsUIDelegate::ForeignSessionFilter::kAll));
  EXPECT_THAT(empty_sessions, IsEmpty());

  const base::Time kTime0 = base::Time::Now();
  ON_CALL(mock_sync_sessions_client_, ShouldSyncURL(GURL("chrome://newtab")))
      .WillByDefault(Return(false));

  // Foreign session with 0 tabs (oldest).
  session_tracker_.GetSession(kSessionTag1)->SetSessionName("device1");
  session_tracker_.GetSession(kSessionTag1)
      ->SetModifiedTime(kTime0 - base::Seconds(3));

  // Foreign session with only a non-syncable/NTP tab (most recent).
  sessions::SerializedNavigationEntry ntp_entry =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  ntp_entry.set_virtual_url(GURL("chrome://newtab"));
  ntp_entry.set_title(u"New Tab");
  session_tracker_.PutWindowInSession(kSessionTag2, kWindowId2);
  session_tracker_.PutTabInWindow(kSessionTag2, kWindowId2, kTabId2);
  session_tracker_.GetTab(kSessionTag2, kTabId2)
      ->navigations.push_back(ntp_entry);
  session_tracker_.GetSession(kSessionTag2)->SetSessionName("device2");
  session_tracker_.GetSession(kSessionTag2)
      ->SetModifiedTime(kTime0 - base::Seconds(1));

  // Foreign session with a syncable tab (middle recency).
  sessions::SerializedNavigationEntry syncable_entry =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  syncable_entry.set_virtual_url(GURL("http://url3"));
  syncable_entry.set_title(u"title3");
  session_tracker_.PutWindowInSession(kSessionTag3, kWindowId3);
  session_tracker_.PutTabInWindow(kSessionTag3, kWindowId3, kTabId3);
  session_tracker_.GetTab(kSessionTag3, kTabId3)
      ->navigations.push_back(syncable_entry);
  session_tracker_.GetSession(kSessionTag3)->SetSessionName("device3");
  session_tracker_.GetSession(kSessionTag3)
      ->SetModifiedTime(kTime0 - base::Seconds(2));

  std::vector<raw_ptr<const SyncedSession, DanglingUntriaged>>
      presentable_sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(
      &presentable_sessions,
      OpenTabsUIDelegate::ForeignSessionFilter::kPresentable));
  EXPECT_THAT(presentable_sessions, ElementsAre(HasSessionTag(kSessionTag3)));

  std::vector<raw_ptr<const SyncedSession, DanglingUntriaged>> all_sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(
      &all_sessions, OpenTabsUIDelegate::ForeignSessionFilter::kAll));
  EXPECT_THAT(all_sessions, ElementsAre(HasSessionTag(kSessionTag2),
                                        HasSessionTag(kSessionTag3),
                                        HasSessionTag(kSessionTag1)));
}

// Verifies that `GetAllForeignSessions()` with `ForeignSessionFilter::kAll`
// includes foreign sessions with empty windows and excludes local, recent
// local, and deleted sessions.
TEST_F(
    OpenTabsUIDelegateImplTest,
    GetAllForeignSessionsWithFilterAllIncludesEmptyWindowAndExcludesLocalAndDeleted) {
  session_tracker_.InitLocalSession("local_tag", "Local Device",
                                    syncer::DeviceInfo::DeviceType::kLinux,
                                    syncer::DeviceInfo::FormFactor::kDesktop);
  ON_CALL(mock_sync_sessions_client_,
          IsRecentLocalCacheGuid("recent_local_tag"))
      .WillByDefault(Return(true));
  session_tracker_.GetSession("recent_local_tag");

  // Foreign session with an empty window (no tabs).
  session_tracker_.GetSession(kSessionTag1)->SetSessionName("device1");
  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId1);

  // Foreign session that gets deleted.
  session_tracker_.GetSession(kSessionTag2)->SetSessionName("device2");
  session_tracker_.DeleteForeignSession(kSessionTag2);

  std::vector<raw_ptr<const SyncedSession, DanglingUntriaged>>
      presentable_sessions;
  EXPECT_FALSE(delegate_.GetAllForeignSessions(
      &presentable_sessions,
      OpenTabsUIDelegate::ForeignSessionFilter::kPresentable));
  EXPECT_THAT(presentable_sessions, IsEmpty());

  std::vector<raw_ptr<const SyncedSession, DanglingUntriaged>> all_sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(
      &all_sessions, OpenTabsUIDelegate::ForeignSessionFilter::kAll));
  EXPECT_THAT(all_sessions, ElementsAre(HasSessionTag(kSessionTag1)));

  session_tracker_.DeleteForeignSession(kSessionTag1);
  EXPECT_FALSE(delegate_.GetAllForeignSessions(
      &all_sessions, OpenTabsUIDelegate::ForeignSessionFilter::kAll));
  EXPECT_THAT(all_sessions, IsEmpty());
}

// Verifies that `GetAllForeignSessions()` with `ForeignSessionFilter::kAll`
// excludes unnamed (headerless) foreign sessions, including orphan tab-only
// sessions that have not received a header.
TEST_F(OpenTabsUIDelegateImplTest,
       GetAllForeignSessionsWithFilterAllExcludesUnnamedSessions) {
  // Headerless/unnamed orphan tab-only session (empty session name).
  session_tracker_.GetTab(kSessionTag1, kTabId1)
      ->navigations.push_back(sessions::SerializedNavigationEntryTestHelper::
                                  CreateNavigationForTest());

  std::vector<raw_ptr<const SyncedSession, DanglingUntriaged>> all_sessions;
  EXPECT_FALSE(delegate_.GetAllForeignSessions(
      &all_sessions, OpenTabsUIDelegate::ForeignSessionFilter::kAll));
  EXPECT_THAT(all_sessions, IsEmpty());

  // Named 0-tab session alongside the unnamed session.
  session_tracker_.GetSession(kSessionTag2)->SetSessionName("device2");
  EXPECT_TRUE(delegate_.GetAllForeignSessions(
      &all_sessions, OpenTabsUIDelegate::ForeignSessionFilter::kAll));
  EXPECT_THAT(all_sessions, ElementsAre(HasSessionTag(kSessionTag2)));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Verifies that `GetAllForeignSessions()` excludes presentable sessions marked
// `SessionVisibility::kHidden` in `SessionSyncPrefs` when
// `kDeviceTabVisibilitySettings` is enabled and returns false when all
// presentable sessions are hidden.
TEST_F(OpenTabsUIDelegateImplTest,
       ShouldExcludeHiddenSessionsFromGetAllForeignSessions) {
  const base::Time kTime0 = base::Time::Now();

  sessions::SerializedNavigationEntry navigation =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  navigation.set_virtual_url(GURL(kUrl1));

  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId1);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId1, kTabId1);
  session_tracker_.GetTab(kSessionTag1, kTabId1)
      ->navigations.push_back(navigation);
  session_tracker_.GetSession(kSessionTag1)->SetSessionName("device1");
  session_tracker_.GetSession(kSessionTag1)
      ->SetModifiedTime(kTime0 - base::Seconds(1));

  session_tracker_.PutWindowInSession(kSessionTag2, kWindowId2);
  session_tracker_.PutTabInWindow(kSessionTag2, kWindowId2, kTabId2);
  session_tracker_.GetTab(kSessionTag2, kTabId2)
      ->navigations.push_back(navigation);
  session_tracker_.GetSession(kSessionTag2)->SetSessionName("device2");
  session_tracker_.GetSession(kSessionTag2)
      ->SetModifiedTime(kTime0 - base::Seconds(2));

  session_sync_prefs_.SetSessionVisibility(kSessionTag1,
                                           SessionVisibility::kHidden);

  std::vector<raw_ptr<const SyncedSession, DanglingUntriaged>> sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(
      &sessions, OpenTabsUIDelegate::ForeignSessionFilter::kPresentable));
  EXPECT_THAT(sessions, ElementsAre(Pointee(Property(
                            &SyncedSession::GetSessionTag, kSessionTag2))));

  // Hiding the remaining presentable session returns false with an empty list,
  // while `ForeignSessionFilter::kAll` still returns both sessions.
  session_sync_prefs_.SetSessionVisibility(kSessionTag2,
                                           SessionVisibility::kHidden);
  EXPECT_FALSE(delegate_.GetAllForeignSessions(
      &sessions, OpenTabsUIDelegate::ForeignSessionFilter::kPresentable));
  EXPECT_THAT(sessions, IsEmpty());
  EXPECT_TRUE(delegate_.GetAllForeignSessions(
      &sessions, OpenTabsUIDelegate::ForeignSessionFilter::kAll));
  EXPECT_THAT(
      sessions,
      ElementsAre(
          Pointee(Property(&SyncedSession::GetSessionTag, kSessionTag1)),
          Pointee(Property(&SyncedSession::GetSessionTag, kSessionTag2))));

  // Unhiding `kSessionTag1` makes it presentable again.
  session_sync_prefs_.SetSessionVisibility(kSessionTag1,
                                           SessionVisibility::kVisible);
  EXPECT_TRUE(delegate_.GetAllForeignSessions(
      &sessions, OpenTabsUIDelegate::ForeignSessionFilter::kPresentable));
  EXPECT_THAT(sessions, ElementsAre(Pointee(Property(
                            &SyncedSession::GetSessionTag, kSessionTag1))));
}

// Verifies that when `kDeviceTabVisibilitySettings` is disabled, sessions
// previously marked hidden in `SessionSyncPrefs` remain visible in
// `GetAllForeignSessions()`.
TEST_F(OpenTabsUIDelegateImplTest,
       ShouldIgnoreHiddenSessionsWhenDeviceTabVisibilityDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      syncer::kDeviceTabVisibilitySettings);

  sessions::SerializedNavigationEntry navigation =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  navigation.set_virtual_url(GURL(kUrl1));

  session_tracker_.PutWindowInSession(kSessionTag1, kWindowId1);
  session_tracker_.PutTabInWindow(kSessionTag1, kWindowId1, kTabId1);
  session_tracker_.GetTab(kSessionTag1, kTabId1)
      ->navigations.push_back(navigation);

  session_sync_prefs_.SetSessionVisibility(kSessionTag1,
                                           SessionVisibility::kHidden);

  std::vector<raw_ptr<const SyncedSession, DanglingUntriaged>> sessions;
  EXPECT_TRUE(delegate_.GetAllForeignSessions(
      &sessions, OpenTabsUIDelegate::ForeignSessionFilter::kPresentable));
  EXPECT_THAT(sessions, ElementsAre(Pointee(Property(
                            &SyncedSession::GetSessionTag, kSessionTag1))));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace
}  // namespace sync_sessions
