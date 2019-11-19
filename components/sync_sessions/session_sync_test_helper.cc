// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_sync_test_helper.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_sessions/synced_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {

static const char* kClientName = "name";
static const char* kAppId = "app_id";
static const char* kVirtualUrl = "http://foo/1";
static const char* kReferrer = "referrer";
static const char* kTitle = "title";

// static
void SessionSyncTestHelper::BuildSessionSpecifics(
    const std::string& tag,
    sync_pb::SessionSpecifics* meta) {
  meta->set_session_tag(tag);
  sync_pb::SessionHeader* header = meta->mutable_header();
  header->set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
  header->set_client_name(kClientName);
}

// static
void SessionSyncTestHelper::AddWindowSpecifics(
    SessionID window_id,
    const std::vector<SessionID>& tab_list,
    sync_pb::SessionSpecifics* meta) {
  sync_pb::SessionHeader* header = meta->mutable_header();
  sync_pb::SessionWindow* window = header->add_window();
  window->set_window_id(window_id.id());
  window->set_selected_tab_index(0);
  window->set_browser_type(sync_pb::SessionWindow_BrowserType_TYPE_TABBED);
  for (auto iter = tab_list.begin(); iter != tab_list.end(); ++iter) {
    window->add_tab(iter->id());
  }
}

// static
void SessionSyncTestHelper::VerifySyncedSession(
    const std::string& tag,
    const std::vector<std::vector<SessionID>>& windows,
    const SyncedSession& session) {
  ASSERT_EQ(tag, session.session_tag);
  ASSERT_EQ(sync_pb::SyncEnums_DeviceType_TYPE_LINUX, session.device_type);
  ASSERT_EQ(kClientName, session.session_name);
  ASSERT_EQ(windows.size(), session.windows.size());

  // We assume the window id's are in increasing order.
  int i = 1;
  for (auto win_iter = windows.begin(); win_iter != windows.end();
       ++win_iter, ++i) {
    sessions::SessionWindow* win_ptr;
    auto map_iter = session.windows.find(SessionID::FromSerializedValue(i));
    if (map_iter != session.windows.end())
      win_ptr = &map_iter->second->wrapped_window;
    else
      FAIL();
    ASSERT_EQ(win_iter->size(), win_ptr->tabs.size());
    ASSERT_EQ(0, win_ptr->selected_tab_index);
    ASSERT_EQ(sessions::SessionWindow::TYPE_NORMAL, win_ptr->type);
    int j = 0;
    for (auto tab_iter = (*win_iter).begin(); tab_iter != (*win_iter).end();
         ++tab_iter, ++j) {
      sessions::SessionTab* tab = win_ptr->tabs[j].get();
      ASSERT_EQ(*tab_iter, tab->tab_id);
      ASSERT_EQ(1U, tab->navigations.size());
      ASSERT_EQ(1, tab->tab_visual_index);
      ASSERT_EQ(0, tab->current_navigation_index);
      ASSERT_TRUE(tab->pinned);
      ASSERT_EQ(kAppId, tab->extension_app_id);
      ASSERT_EQ(1U, tab->navigations.size());
      ASSERT_EQ(tab->navigations[0].virtual_url(), GURL(kVirtualUrl));
      ASSERT_EQ(tab->navigations[0].referrer_url(), GURL(kReferrer));
      ASSERT_EQ(tab->navigations[0].title(), base::ASCIIToUTF16(kTitle));
      ASSERT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
          tab->navigations[0].transition_type(), ui::PAGE_TRANSITION_TYPED));
    }
  }
}

sync_pb::SessionSpecifics SessionSyncTestHelper::BuildTabSpecifics(
    const std::string& tag,
    SessionID window_id,
    SessionID tab_id) {
  return BuildTabSpecifics(tag, window_id, tab_id, ++max_tab_node_id_);
}

sync_pb::SessionSpecifics SessionSyncTestHelper::BuildTabSpecifics(
    const std::string& tag,
    SessionID window_id,
    SessionID tab_id,
    int tab_node_id) {
  sync_pb::SessionSpecifics specifics;
  specifics.set_session_tag(tag);
  specifics.set_tab_node_id(tab_node_id);
  sync_pb::SessionTab* tab = specifics.mutable_tab();
  tab->set_tab_id(tab_id.id());
  tab->set_tab_visual_index(1);
  tab->set_current_navigation_index(0);
  tab->set_pinned(true);
  tab->set_extension_app_id(kAppId);
  sync_pb::TabNavigation* navigation = tab->add_navigation();
  navigation->set_virtual_url(kVirtualUrl);
  navigation->set_referrer(kReferrer);
  navigation->set_title(kTitle);
  navigation->set_page_transition(sync_pb::SyncEnums_PageTransition_TYPED);
  return specifics;
}

void SessionSyncTestHelper::Reset() {
  max_tab_node_id_ = 0;
}

sync_pb::SessionSpecifics SessionSyncTestHelper::BuildForeignSession(
    const std::string& tag,
    const std::vector<SessionID>& tab_list,
    std::vector<sync_pb::SessionSpecifics>* tabs) {
  const SessionID window_id = SessionID::FromSerializedValue(1);
  sync_pb::SessionSpecifics header;
  BuildSessionSpecifics(tag, &header);
  AddWindowSpecifics(window_id, tab_list, &header);

  if (tabs) {
    tabs->clear();
    for (SessionID tab_id : tab_list) {
      tabs->push_back(BuildTabSpecifics(tag, window_id, tab_id));
    }
  }

  return header;
}

}  // namespace sync_sessions
