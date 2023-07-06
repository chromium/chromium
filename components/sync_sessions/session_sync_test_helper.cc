// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_sync_test_helper.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync_device_info/device_info_proto_enum_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {

static const char* kAppId = "app_id";
static const char* kVirtualUrl = "http://foo/1";
static const char* kReferrer = "referrer";
static const char* kTitle = "title";

// static
sync_pb::SessionSpecifics
SessionSyncTestHelper::BuildHeaderSpecificsWithoutWindows(
    const std::string& tag,
    const std::string& client_name,
    const syncer::DeviceInfo::FormFactor& device_form_factor) {
  sync_pb::SessionSpecifics specifics;
  specifics.set_session_tag(tag);
  sync_pb::SessionHeader* header = specifics.mutable_header();
  header->set_device_form_factor(ToDeviceFormFactorProto(device_form_factor));
  header->set_client_name(client_name);
  return specifics;
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
  window->set_browser_type(sync_pb::SyncEnums_BrowserType_TYPE_TABBED);
  for (const SessionID tab_id : tab_list) {
    window->add_tab(tab_id.id());
  }
}

// static
sync_pb::SessionSpecifics SessionSyncTestHelper::BuildTabSpecifics(
    const std::string& tag,
    const std::string& title,
    const std::string& virtual_url,
    SessionID window_id,
    SessionID tab_id,
    int tab_node_id) {
  sync_pb::SessionSpecifics specifics;
  specifics.set_session_tag(tag);
  specifics.set_tab_node_id(tab_node_id);
  sync_pb::SessionTab* tab = specifics.mutable_tab();
  tab->set_window_id(window_id.id());
  tab->set_tab_id(tab_id.id());
  tab->set_tab_visual_index(1);
  tab->set_current_navigation_index(0);
  tab->set_pinned(true);
  tab->set_extension_app_id(kAppId);
  sync_pb::TabNavigation* navigation = tab->add_navigation();
  navigation->set_title(title);
  navigation->set_virtual_url(virtual_url);
  navigation->set_referrer(kReferrer);
  navigation->set_page_transition(sync_pb::SyncEnums_PageTransition_TYPED);
  return specifics;
}

sync_pb::SessionSpecifics SessionSyncTestHelper::BuildTabSpecifics(
    const std::string& tag,
    SessionID window_id,
    SessionID tab_id) {
  return BuildTabSpecifics(tag, kTitle, kVirtualUrl, window_id, tab_id);
}

// Overload of BuildTabSpecifics to allow overriding title and URL.
sync_pb::SessionSpecifics SessionSyncTestHelper::BuildTabSpecifics(
    const std::string& tag,
    const std::string& title,
    const std::string& virtual_url,
    SessionID window_id,
    SessionID tab_id) {
  return BuildTabSpecifics(tag, title, virtual_url, window_id, tab_id,
                           ++max_tab_node_id_);
}

void SessionSyncTestHelper::Reset() {
  max_tab_node_id_ = 0;
}

}  // namespace sync_sessions
