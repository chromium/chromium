// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/recent_tabs_builder_test_helper.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/data_type_processor.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kBaseSessionTag[] = "session_tag";
const char kBaseSessionName[] = "session_name";
const char kBaseTabUrl[] = "http://foo/?";
const char kTabTitleFormat[] = "session=%d;window=%d;tab=%d";
const uint64_t kMaxMinutesRange = 1000;

struct TitleTimestampPair {
  std::u16string title;
  base::Time timestamp;
};

bool SortTabTimesByRecency(const TitleTimestampPair& t1,
                           const TitleTimestampPair& t2) {
  return t1.timestamp > t2.timestamp;
}

std::string ToSessionTag(SessionID session_id) {
  return std::string(kBaseSessionTag + base::NumberToString(session_id.id()));
}

std::string ToSessionName(SessionID session_id) {
  return std::string(kBaseSessionName + base::NumberToString(session_id.id()));
}

std::string ToTabTitle(SessionID session_id,
                       SessionID window_id,
                       SessionID tab_id) {
  return base::StringPrintf(kTabTitleFormat, session_id.id(), window_id.id(),
                            tab_id.id());
}

std::string ToTabUrl(SessionID session_id,
                     SessionID window_id,
                     SessionID tab_id) {
  return std::string(kBaseTabUrl + ToTabTitle(session_id, window_id, tab_id));
}

}  // namespace

struct RecentTabsBuilderTestHelper::TabInfo {
  TabInfo() : id(SessionID::InvalidValue()) {}
  SessionID id;
  base::Time timestamp;
  std::u16string title;
};
struct RecentTabsBuilderTestHelper::WindowInfo {
  WindowInfo() : id(SessionID::InvalidValue()) {}
  ~WindowInfo() {}
  SessionID id;
  std::vector<TabInfo> tabs;
};
struct RecentTabsBuilderTestHelper::SessionInfo {
  SessionInfo() : id(SessionID::InvalidValue()) {}
  ~SessionInfo() {}
  SessionID id;
  std::vector<WindowInfo> windows;
};

RecentTabsBuilderTestHelper::RecentTabsBuilderTestHelper() {
  start_time_ = base::Time::Now();
}

RecentTabsBuilderTestHelper::~RecentTabsBuilderTestHelper() {}

void RecentTabsBuilderTestHelper::AddSession() {
  SessionInfo info;
  info.id = SessionID::NewUnique();
  sessions_.push_back(info);
}

int RecentTabsBuilderTestHelper::GetSessionCount() {
  return sessions_.size();
}

SessionID RecentTabsBuilderTestHelper::GetSessionID(int session_index) {
  return sessions_[session_index].id;
}

base::Time RecentTabsBuilderTestHelper::GetSessionTimestamp(int session_index) {
  std::vector<base::Time> timestamps;
  for (int w = 0; w < GetWindowCount(session_index); ++w) {
    for (int t = 0; t < GetTabCount(session_index, w); ++t) {
      timestamps.push_back(GetTabTimestamp(session_index, w, t));
    }
  }

  if (timestamps.empty()) {
    return base::Time::Now();
  }

  sort(timestamps.begin(), timestamps.end());
  return timestamps[0];
}

void RecentTabsBuilderTestHelper::AddWindow(int session_index) {
  WindowInfo window_info;
  window_info.id = SessionID::NewUnique();
  sessions_[session_index].windows.push_back(window_info);
}

int RecentTabsBuilderTestHelper::GetWindowCount(int session_index) {
  return sessions_[session_index].windows.size();
}

SessionID RecentTabsBuilderTestHelper::GetWindowID(int session_index,
                                                   int window_index) {
  return sessions_[session_index].windows[window_index].id;
}

void RecentTabsBuilderTestHelper::AddTab(int session_index, int window_index) {
  base::Time timestamp =
      start_time_ + base::Minutes(base::RandGenerator(kMaxMinutesRange));
  AddTabWithInfo(session_index, window_index, timestamp, std::u16string());
}

void RecentTabsBuilderTestHelper::AddTabWithInfo(int session_index,
                                                 int window_index,
                                                 base::Time timestamp,
                                                 const std::u16string& title) {
  TabInfo tab_info;
  tab_info.id = SessionID::NewUnique();
  tab_info.timestamp = timestamp;
  tab_info.title = title;
  sessions_[session_index].windows[window_index].tabs.push_back(tab_info);
}

int RecentTabsBuilderTestHelper::GetTabCount(int session_index,
                                             int window_index) {
  return sessions_[session_index].windows[window_index].tabs.size();
}

SessionID RecentTabsBuilderTestHelper::GetTabID(int session_index,
                                                int window_index,
                                                int tab_index) {
  return sessions_[session_index].windows[window_index].tabs[tab_index].id;
}

base::Time RecentTabsBuilderTestHelper::GetTabTimestamp(int session_index,
                                                        int window_index,
                                                        int tab_index) {
  return sessions_[session_index]
      .windows[window_index]
      .tabs[tab_index]
      .timestamp;
}

std::u16string RecentTabsBuilderTestHelper::GetTabTitle(int session_index,
                                                        int window_index,
                                                        int tab_index) {
  std::u16string title =
      sessions_[session_index].windows[window_index].tabs[tab_index].title;
  if (title.empty()) {
    title = base::UTF8ToUTF16(ToTabTitle(
        GetSessionID(session_index), GetWindowID(session_index, window_index),
        GetTabID(session_index, window_index, tab_index)));
  }
  return title;
}

void RecentTabsBuilderTestHelper::ExportToSessionSync(
    syncer::DataTypeProcessor* processor) {
  syncer::UpdateResponseDataList updates;

  for (int s = 0; s < GetSessionCount(); ++s) {
    sync_pb::SessionSpecifics header_specifics = BuildHeaderSpecifics(s);
    for (int w = 0; w < GetWindowCount(s); ++w) {
      AddWindowToHeaderSpecifics(s, w, &header_specifics);
      for (int t = 0; t < GetTabCount(s, w); ++t) {
        updates.push_back(BuildUpdateResponseData(BuildTabSpecifics(s, w, t),
                                                  GetTabTimestamp(s, w, t)));
      }
    }

    updates.push_back(
        BuildUpdateResponseData(header_specifics, GetSessionTimestamp(s)));
  }

  sync_pb::DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  processor->OnUpdateReceived(data_type_state, std::move(updates),
                              /*gc_directive=*/std::nullopt);
  // ClientTagBasedDataTypeProcessor uses DataTypeProcessorProxy during
  // activation, which involves task posting for receiving updates.
  base::RunLoop().RunUntilIdle();
}

void RecentTabsBuilderTestHelper::VerifyExport(
    sync_sessions::OpenTabsUIDelegate* delegate) {
  DCHECK(delegate);
  // Make sure data is populated correctly in SessionModelAssociator.
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  ASSERT_TRUE(delegate->GetAllForeignSessions(&sessions));
  ASSERT_EQ(GetSessionCount(), static_cast<int>(sessions.size()));
  for (int s = 0; s < GetSessionCount(); ++s) {
    std::vector<const sessions::SessionWindow*> windows =
        delegate->GetForeignSession(ToSessionTag(GetSessionID(s)));
    ASSERT_EQ(GetWindowCount(s), static_cast<int>(windows.size()));
    for (int w = 0; w < GetWindowCount(s); ++w) {
      ASSERT_EQ(GetTabCount(s, w), static_cast<int>(windows[w]->tabs.size()));
    }
  }
}

std::vector<std::u16string>
RecentTabsBuilderTestHelper::GetTabTitlesSortedByRecency() {
  std::vector<TitleTimestampPair> tabs;
  for (int s = 0; s < GetSessionCount(); ++s) {
    for (int w = 0; w < GetWindowCount(s); ++w) {
      for (int t = 0; t < GetTabCount(s, w); ++t) {
        TitleTimestampPair pair;
        pair.title = GetTabTitle(s, w, t);
        pair.timestamp = GetTabTimestamp(s, w, t);
        tabs.push_back(pair);
      }
    }
  }
  sort(tabs.begin(), tabs.end(), SortTabTimesByRecency);

  std::vector<std::u16string> titles;
  for (size_t i = 0; i < tabs.size(); ++i) {
    titles.push_back(tabs[i].title);
  }
  return titles;
}

sync_pb::SessionSpecifics RecentTabsBuilderTestHelper::BuildHeaderSpecifics(
    int session_index) {
  sync_pb::SessionSpecifics specifics;
  SessionID session_id = GetSessionID(session_index);
  specifics.set_session_tag(ToSessionTag(session_id));
  sync_pb::SessionHeader* header = specifics.mutable_header();
  header->set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_CROS);
  header->set_client_name(ToSessionName(session_id));
  return specifics;
}

void RecentTabsBuilderTestHelper::AddWindowToHeaderSpecifics(
    int session_index,
    int window_index,
    sync_pb::SessionSpecifics* specifics) {
  sync_pb::SessionWindow* window = specifics->mutable_header()->add_window();
  SessionID window_id = GetWindowID(session_index, window_index);
  window->set_window_id(window_id.id());
  window->set_selected_tab_index(0);
  window->set_browser_type(sync_pb::SyncEnums_BrowserType_TYPE_TABBED);
  for (int i = 0; i < GetTabCount(session_index, window_index); ++i) {
    window->add_tab(GetTabID(session_index, window_index, i).id());
  }
}

sync_pb::SessionSpecifics RecentTabsBuilderTestHelper::BuildTabSpecifics(
    int session_index,
    int window_index,
    int tab_index) {
  sync_pb::SessionSpecifics specifics;

  SessionID session_id = GetSessionID(session_index);
  SessionID window_id = GetWindowID(session_index, window_index);
  SessionID tab_id = GetTabID(session_index, window_index, tab_index);

  specifics.set_session_tag(ToSessionTag(session_id));
  specifics.set_tab_node_id(++max_tab_node_id_);
  sync_pb::SessionTab* tab = specifics.mutable_tab();
  tab->set_window_id(window_id.id());
  tab->set_tab_id(tab_id.id());
  tab->set_tab_visual_index(1);
  tab->set_current_navigation_index(0);
  tab->set_pinned(true);
  tab->set_extension_app_id("app_id");
  sync_pb::TabNavigation* navigation = tab->add_navigation();
  navigation->set_virtual_url(ToTabUrl(session_id, window_id, tab_id));
  navigation->set_referrer("referrer");
  navigation->set_title(
      base::UTF16ToUTF8(GetTabTitle(session_index, window_index, tab_index)));
  navigation->set_page_transition(sync_pb::SyncEnums_PageTransition_TYPED);

  return specifics;
}

syncer::UpdateResponseData RecentTabsBuilderTestHelper::BuildUpdateResponseData(
    const sync_pb::SessionSpecifics& specifics,
    base::Time timestamp) {
  syncer::EntityData entity;
  *entity.specifics.mutable_session() = specifics;
  entity.creation_time = timestamp;
  entity.modification_time = timestamp;
  entity.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
      syncer::SESSIONS, sync_sessions::SessionStore::GetClientTag(specifics));
  entity.id = entity.client_tag_hash.value();

  syncer::UpdateResponseData update;
  update.entity = std::move(entity);
  update.response_version = ++next_response_version_;
  return update;
}
