// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session.h"

#include <cstddef>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace sync_sessions {
namespace {

namespace test_data = sessions::test_data;

using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;

// Create a sync_pb::TabNavigation from the constants above.
sync_pb::TabNavigation MakeSyncDataForTest() {
  sync_pb::TabNavigation sync_data;
  sync_data.set_virtual_url("http://www.virtual-url.com/");
  sync_data.set_referrer("http://www.referrer.com/");
  sync_data.set_obsolete_referrer_policy(test_data::kReferrerPolicy);
  sync_data.set_correct_referrer_policy(test_data::kReferrerPolicy);
  sync_data.set_title(base::UTF16ToUTF8(test_data::kTitle));
  sync_data.set_page_transition(
      sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME);
  sync_data.set_unique_id(test_data::kUniqueID);
  sync_data.set_timestamp_msec(syncer::TimeToProtoTime(test_data::kTimestamp));
  sync_data.set_redirect_type(sync_pb::SyncEnums::CLIENT_REDIRECT);
  sync_data.set_navigation_home_page(true);
  sync_data.set_favicon_url("http://virtual-url.com/favicon.ico");
  sync_data.set_http_status_code(test_data::kHttpStatusCode);
  // The redirect chain only syncs one way.
  return sync_data;
}

// Create a SerializedNavigationEntry from a sync_pb::TabNavigation.  All its
// fields should match the protocol buffer's if it exists there, and
// should be set to the default value otherwise.
TEST(SyncedSessionTest, SessionNavigationFromSyncData) {
  const sync_pb::TabNavigation sync_data = MakeSyncDataForTest();

  const SerializedNavigationEntry navigation =
      SessionNavigationFromSyncData(test_data::kIndex, sync_data);

  EXPECT_EQ(test_data::kIndex, navigation.index());
  EXPECT_EQ(sync_data.unique_id(), navigation.unique_id());
  EXPECT_EQ(sync_data.referrer(), navigation.referrer_url().spec());
  EXPECT_EQ(sync_data.correct_referrer_policy(), navigation.referrer_policy());
  EXPECT_EQ(sync_data.virtual_url(), navigation.virtual_url().spec());
  EXPECT_EQ(base::UTF8ToUTF16(sync_data.title()), navigation.title());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), test_data::kTransitionType));
  EXPECT_FALSE(navigation.has_post_data());
  EXPECT_EQ(-1, navigation.post_id());
  EXPECT_EQ(GURL(), navigation.original_request_url());
  EXPECT_FALSE(navigation.is_overriding_user_agent());
  EXPECT_EQ(sync_data.timestamp_msec(),
            syncer::TimeToProtoTime(navigation.timestamp()));
  EXPECT_EQ(sync_data.favicon_url(), navigation.favicon_url().spec());
  EXPECT_EQ(sync_data.http_status_code(), navigation.http_status_code());
  // The redirect chain only syncs one way.
}

// Create a SerializedNavigationEntry, then create a sync protocol buffer from
// it.  The protocol buffer should have matching fields to the
// SerializedNavigationEntry (when applicable).
TEST(SyncedSessionTest, SessionNavigationToSyncData) {
  const SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  const sync_pb::TabNavigation sync_data =
      SessionNavigationToSyncData(navigation);

  EXPECT_EQ(navigation.virtual_url().spec(), sync_data.virtual_url());
  EXPECT_EQ(navigation.referrer_url().spec(), sync_data.referrer());
  EXPECT_EQ(navigation.title(), base::UTF8ToUTF16(sync_data.title()));
  EXPECT_EQ(sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME,
            sync_data.page_transition());
  EXPECT_TRUE(sync_data.has_redirect_type());
  EXPECT_EQ(navigation.unique_id(), sync_data.unique_id());
  EXPECT_EQ(syncer::TimeToProtoTime(navigation.timestamp()),
            sync_data.timestamp_msec());
  EXPECT_EQ(navigation.favicon_url().spec(), sync_data.favicon_url());
  EXPECT_EQ(navigation.http_status_code(), sync_data.http_status_code());
}

// Ensure all transition types and qualifiers are converted to/from the sync
// SerializedNavigationEntry representation properly.
TEST(SyncedSessionTest, SessionNavigationToSyncDataWithTransitionTypes) {
  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();

  for (uint32_t core_type = ui::PAGE_TRANSITION_LINK;
       core_type < ui::PAGE_TRANSITION_LAST_CORE; ++core_type) {
    // Because qualifier is a uint32_t, left shifting will eventually overflow
    // and hit zero again. SERVER_REDIRECT, as the last qualifier and also
    // in place of the sign bit, is therefore the last transition before
    // breaking.
    for (uint32_t qualifier = ui::PAGE_TRANSITION_FORWARD_BACK; qualifier != 0;
         qualifier <<= 1) {
      if (qualifier == static_cast<uint32_t>(ui::PAGE_TRANSITION_FROM_API) ||
          qualifier == static_cast<uint32_t>(ui::PAGE_TRANSITION_CHAIN_START) ||
          qualifier == static_cast<uint32_t>(ui::PAGE_TRANSITION_CHAIN_END)) {
        continue;  // We don't sync PAGE_TRANSITION_FROM_API or CHAIN_START/END.
      }
      ui::PageTransition transition =
          ui::PageTransitionFromInt(core_type | qualifier);
      SerializedNavigationEntryTestHelper::SetTransitionType(transition,
                                                             &navigation);

      const sync_pb::TabNavigation sync_data =
          SessionNavigationToSyncData(navigation);
      const SerializedNavigationEntry constructed_nav =
          SessionNavigationFromSyncData(test_data::kIndex, sync_data);
      const ui::PageTransition constructed_transition =
          constructed_nav.transition_type();

      EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
          constructed_transition, transition));
    }
  }
}

TEST(SyncedSessionTest, SessionNavigationToSyncDataWithLargeFavicon) {
  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();

  ASSERT_TRUE(SessionNavigationToSyncData(navigation).has_favicon_url());

  // The URL size is greater than |kMaxFaviconUrlSizeToSync| so it will be
  // omitted.
  navigation.set_favicon_url(
      GURL(std::string("http://virtual-url.com/") + std::string(2048, 'z')));

  const sync_pb::TabNavigation sync_data =
      SessionNavigationToSyncData(navigation);

  EXPECT_FALSE(sync_data.has_favicon_url());

  // The rest of the fields should sync normally, let's verify one of them.
  EXPECT_EQ(navigation.virtual_url().spec(), sync_data.virtual_url());
}

// Create a typical SessionTab protocol buffer and set an existing
// SessionTab from it.  The data from the protocol buffer should
// clobber the existing data.
TEST(SyncedSessionTest, SetSessionTabFromSyncData) {
  sync_pb::SessionTab sync_data;
  sync_data.set_tab_id(5);
  sync_data.set_window_id(10);
  sync_data.set_tab_visual_index(13);
  sync_data.set_current_navigation_index(3);
  sync_data.set_pinned(true);
  sync_data.set_extension_app_id("app_id");
  for (int i = 0; i < 5; ++i) {
    sync_pb::TabNavigation* navigation = sync_data.add_navigation();
    navigation->set_virtual_url("http://foo/" + base::NumberToString(i));
    navigation->set_referrer("referrer");
    navigation->set_title("title");
    navigation->set_page_transition(sync_pb::SyncEnums_PageTransition_TYPED);
  }

  sessions::SessionTab tab;
  tab.window_id = SessionID::FromSerializedValue(100);
  tab.tab_id = SessionID::FromSerializedValue(100);
  tab.tab_visual_index = 100;
  tab.current_navigation_index = 1000;
  tab.pinned = false;
  tab.extension_app_id = "fake";
  tab.user_agent_override.ua_string_override = "fake";
  tab.timestamp = base::Time::FromInternalValue(100);
  tab.navigations.resize(100);
  tab.session_storage_persistent_id = "fake";

  SetSessionTabFromSyncData(sync_data, base::Time::FromInternalValue(5u), &tab);
  EXPECT_EQ(10, tab.window_id.id());
  EXPECT_EQ(5, tab.tab_id.id());
  EXPECT_EQ(13, tab.tab_visual_index);
  EXPECT_EQ(3, tab.current_navigation_index);
  EXPECT_TRUE(tab.pinned);
  EXPECT_EQ("app_id", tab.extension_app_id);
  EXPECT_TRUE(tab.user_agent_override.ua_string_override.empty());
  EXPECT_EQ(5u, tab.timestamp.ToInternalValue());
  ASSERT_EQ(5u, tab.navigations.size());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(i, tab.navigations[i].index());
    EXPECT_EQ(GURL("referrer"), tab.navigations[i].referrer_url());
    EXPECT_EQ(u"title", tab.navigations[i].title());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        tab.navigations[i].transition_type(), ui::PAGE_TRANSITION_TYPED));
    EXPECT_EQ(GURL("http://foo/" + base::NumberToString(i)),
              tab.navigations[i].virtual_url());
  }
  EXPECT_TRUE(tab.session_storage_persistent_id.empty());
}

TEST(SyncedSessionTest, SessionTabToSyncData) {
  sessions::SessionTab tab;
  tab.window_id = SessionID::FromSerializedValue(10);
  tab.tab_id = SessionID::FromSerializedValue(5);
  tab.tab_visual_index = 13;
  tab.current_navigation_index = 3;
  tab.pinned = true;
  tab.extension_app_id = "app_id";
  tab.user_agent_override.ua_string_override = "fake";
  tab.timestamp = base::Time::FromInternalValue(100);
  for (int i = 0; i < 5; ++i) {
    sessions::SerializedNavigationEntry entry =
        SerializedNavigationEntryTestHelper::CreateNavigationForTest();
    entry.set_virtual_url(GURL("http://foo/" + base::NumberToString(i)));
    entry.set_title(base::UTF8ToUTF16("title" + base::NumberToString(i)));
    tab.navigations.push_back(entry);
  }
  tab.session_storage_persistent_id = "fake";

  const sync_pb::SessionTab sync_data =
      SessionTabToSyncData(tab, /*browser_type=*/std::nullopt);
  EXPECT_EQ(5, sync_data.tab_id());
  EXPECT_EQ(10, sync_data.window_id());
  EXPECT_EQ(13, sync_data.tab_visual_index());
  EXPECT_EQ(3, sync_data.current_navigation_index());
  EXPECT_TRUE(sync_data.pinned());
  EXPECT_EQ("app_id", sync_data.extension_app_id());
  ASSERT_EQ(5, sync_data.navigation_size());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(tab.navigations[i].virtual_url().spec(),
              sync_data.navigation(i).virtual_url());
    EXPECT_EQ(base::UTF16ToUTF8(tab.navigations[i].title()),
              sync_data.navigation(i).title());
  }
  EXPECT_FALSE(sync_data.has_favicon());
  EXPECT_FALSE(sync_data.has_favicon_type());
  EXPECT_FALSE(sync_data.has_favicon_source());
  EXPECT_FALSE(sync_data.has_browser_type());
}

TEST(SyncedSessionTest, SessionTabToSyncDataWithBrowserType) {
  EXPECT_EQ(sync_pb::SyncEnums_BrowserType_TYPE_TABBED,
            SessionTabToSyncData(sessions::SessionTab(),
                                 sync_pb::SyncEnums_BrowserType_TYPE_TABBED)
                .browser_type());
  EXPECT_EQ(sync_pb::SyncEnums_BrowserType_TYPE_POPUP,
            SessionTabToSyncData(sessions::SessionTab(),
                                 sync_pb::SyncEnums_BrowserType_TYPE_POPUP)
                .browser_type());
}

}  // namespace
}  // namespace sync_sessions
