// Copyright 2012 The Chromium Authors. All rights reserved.
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
  sync_data.set_virtual_url(test_data::kVirtualURL.spec());
  sync_data.set_referrer(test_data::kReferrerURL.spec());
  sync_data.set_obsolete_referrer_policy(test_data::kReferrerPolicy);
  sync_data.set_correct_referrer_policy(test_data::kReferrerPolicy);
  sync_data.set_title(base::UTF16ToUTF8(test_data::kTitle));
  sync_data.set_page_transition(
      sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME);
  sync_data.set_unique_id(test_data::kUniqueID);
  sync_data.set_timestamp_msec(syncer::TimeToProtoTime(test_data::kTimestamp));
  sync_data.set_redirect_type(sync_pb::SyncEnums::CLIENT_REDIRECT);
  sync_data.set_navigation_home_page(true);
  sync_data.set_favicon_url(test_data::kFaviconURL.spec());
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
  EXPECT_EQ(test_data::kUniqueID, navigation.unique_id());
  EXPECT_EQ(test_data::kReferrerURL, navigation.referrer_url());
  EXPECT_EQ(test_data::kReferrerPolicy, navigation.referrer_policy());
  EXPECT_EQ(test_data::kVirtualURL, navigation.virtual_url());
  EXPECT_EQ(test_data::kTitle, navigation.title());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), test_data::kTransitionType));
  EXPECT_FALSE(navigation.has_post_data());
  EXPECT_EQ(-1, navigation.post_id());
  EXPECT_EQ(GURL(), navigation.original_request_url());
  EXPECT_FALSE(navigation.is_overriding_user_agent());
  EXPECT_EQ(test_data::kTimestamp, navigation.timestamp());
  EXPECT_EQ(test_data::kFaviconURL, navigation.favicon_url());
  EXPECT_EQ(test_data::kHttpStatusCode, navigation.http_status_code());
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

  EXPECT_EQ(test_data::kVirtualURL.spec(), sync_data.virtual_url());
  EXPECT_EQ(test_data::kReferrerURL.spec(), sync_data.referrer());
  EXPECT_EQ(test_data::kTitle, base::ASCIIToUTF16(sync_data.title()));
  EXPECT_EQ(sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME,
            sync_data.page_transition());
  EXPECT_TRUE(sync_data.has_redirect_type());
  EXPECT_EQ(test_data::kUniqueID, sync_data.unique_id());
  EXPECT_EQ(syncer::TimeToProtoTime(test_data::kTimestamp),
            sync_data.timestamp_msec());
  EXPECT_EQ(test_data::kTimestamp.ToInternalValue(), sync_data.global_id());
  EXPECT_EQ(test_data::kFaviconURL.spec(), sync_data.favicon_url());
  EXPECT_EQ(test_data::kHttpStatusCode, sync_data.http_status_code());
  // The proto navigation redirects don't include the final chain entry
  // (because it didn't redirect) so the lengths should differ by 1.
  ASSERT_EQ(3, sync_data.navigation_redirect_size() + 1);
  EXPECT_EQ(test_data::kRedirectURL0.spec(),
            sync_data.navigation_redirect(0).url());
  EXPECT_EQ(test_data::kRedirectURL1.spec(),
            sync_data.navigation_redirect(1).url());
  EXPECT_FALSE(sync_data.has_last_navigation_redirect_url());
  EXPECT_FALSE(sync_data.has_replaced_navigation());
}

// Specifically test the |replaced_navigation| field, which should be populated
// when the navigation entry has been replaced by another entry (e.g.
// history.pushState()).
TEST(SyncedSessionTest, SessionNavigationToSyncDataWithReplacedNavigation) {
  const GURL kReplacedURL = GURL("http://replaced-url.com");
  const int kReplacedTimestampMs = 79;
  const ui::PageTransition kReplacedPageTransition =
      ui::PAGE_TRANSITION_AUTO_BOOKMARK;

  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  SerializedNavigationEntryTestHelper::SetReplacedEntryData(
      {kReplacedURL, syncer::ProtoTimeToTime(kReplacedTimestampMs),
       kReplacedPageTransition},
      &navigation);

  const sync_pb::TabNavigation sync_data =
      SessionNavigationToSyncData(navigation);
  EXPECT_TRUE(sync_data.has_replaced_navigation());
  EXPECT_EQ(kReplacedURL.spec(),
            sync_data.replaced_navigation().first_committed_url());
  EXPECT_EQ(kReplacedTimestampMs,
            sync_data.replaced_navigation().first_timestamp_msec());
  EXPECT_EQ(sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK,
            sync_data.replaced_navigation().first_page_transition());
}

// Test that the last_navigation_redirect_url is set when needed.  This test is
// just like the above, but with a different virtual_url.  Create a
// SerializedNavigationEntry, then create a sync protocol buffer from it.  The
// protocol buffer should have a last_navigation_redirect_url.
TEST(SyncedSessionTest, SessionNavigationToSyncDataWithLastRedirectUrl) {
  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  SerializedNavigationEntryTestHelper::SetVirtualURL(test_data::kOtherURL,
                                                     &navigation);

  const sync_pb::TabNavigation sync_data =
      SessionNavigationToSyncData(navigation);
  EXPECT_TRUE(sync_data.has_last_navigation_redirect_url());
  EXPECT_EQ(test_data::kVirtualURL.spec(),
            sync_data.last_navigation_redirect_url());

  // The redirect chain should be the same as in the above test.
  ASSERT_EQ(3, sync_data.navigation_redirect_size() + 1);
  EXPECT_EQ(test_data::kRedirectURL0.spec(),
            sync_data.navigation_redirect(0).url());
  EXPECT_EQ(test_data::kRedirectURL1.spec(),
            sync_data.navigation_redirect(1).url());
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
      if (qualifier == static_cast<uint32_t>(ui::PAGE_TRANSITION_FROM_API))
        continue;  // We don't sync PAGE_TRANSITION_FROM_API.
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
  tab.user_agent_override = "fake";
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
  EXPECT_TRUE(tab.user_agent_override.empty());
  EXPECT_EQ(5u, tab.timestamp.ToInternalValue());
  ASSERT_EQ(5u, tab.navigations.size());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(i, tab.navigations[i].index());
    EXPECT_EQ(GURL("referrer"), tab.navigations[i].referrer_url());
    EXPECT_EQ(base::ASCIIToUTF16("title"), tab.navigations[i].title());
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
  tab.user_agent_override = "fake";
  tab.timestamp = base::Time::FromInternalValue(100);
  for (int i = 0; i < 5; ++i) {
    sessions::SerializedNavigationEntry entry =
        SerializedNavigationEntryTestHelper::CreateNavigationForTest();
    entry.set_virtual_url(GURL("http://foo/" + base::NumberToString(i)));
    entry.set_title(base::UTF8ToUTF16("title" + base::NumberToString(i)));
    tab.navigations.push_back(entry);
  }
  tab.session_storage_persistent_id = "fake";

  const sync_pb::SessionTab sync_data = SessionTabToSyncData(tab);
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
}

}  // namespace
}  // namespace sync_sessions
