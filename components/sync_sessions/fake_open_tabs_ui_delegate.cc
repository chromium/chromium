// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/fake_open_tabs_ui_delegate.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/notimplemented.h"

namespace sync_sessions {

namespace {

std::vector<const sessions::SessionWindow*> GetSessionWindows(
    const SyncedSession& session) {
  std::vector<const sessions::SessionWindow*> windows;
  for (const auto& [window_id, window] : session.windows) {
    windows.push_back(&window->wrapped_window);
  }
  return windows;
}

}  // namespace

FakeOpenTabsUIDelegate::FakeOpenTabsUIDelegate()
    : local_session_(std::make_unique<SyncedSession>()) {}

FakeOpenTabsUIDelegate::~FakeOpenTabsUIDelegate() = default;

bool FakeOpenTabsUIDelegate::GetAllForeignSessions(
    std::vector<raw_ptr<const SyncedSession, VectorExperimental>>* sessions) {
  for (const auto& session : foreign_sessions_) {
    sessions->push_back(session.get());
  }
  std::ranges::sort(
      *sessions, std::greater(),
      [](const SyncedSession* session) { return session->GetModifiedTime(); });
  return !foreign_sessions_.empty();
}

base::flat_map<std::string, base::Time>
FakeOpenTabsUIDelegate::GetAllForeignSessionLastModifiedTimes() const {
  return base::MakeFlatMap<std::string, base::Time>(
      foreign_sessions_, std::less<>(), [](const auto& session) {
        return std::make_pair(session->GetSessionTag(),
                              session->GetModifiedTime());
      });
}

bool FakeOpenTabsUIDelegate::GetForeignTab(const std::string& tag,
                                           const SessionID tab_id,
                                           const sessions::SessionTab** tab) {
  // TODO(crbug.com/512514751): This is a workaround because some tests rely on
  // the ability to access the local session from this function.
  if (local_session_ && local_session_->GetSessionTag() == tag) {
    for (const auto& [window_id, window] : local_session_->windows) {
      for (const auto& t : window->wrapped_window.tabs) {
        if (t->tab_id == tab_id) {
          *tab = t.get();
          return true;
        }
      }
    }
  }

  if (SyncedSession* session = FindForeignSession(tag)) {
    for (const auto& [window_id, window] : session->windows) {
      for (const auto& t : window->wrapped_window.tabs) {
        if (t->tab_id == tab_id) {
          *tab = t.get();
          return true;
        }
      }
    }
  }
  return false;
}

void FakeOpenTabsUIDelegate::DeleteForeignSession(const std::string& tag) {
  std::erase_if(foreign_sessions_, [&tag](const auto& session) {
    return session->GetSessionTag() == tag;
  });
}

std::vector<const sessions::SessionWindow*>
FakeOpenTabsUIDelegate::GetForeignSession(const std::string& tag) {
  if (local_session_ && local_session_->GetSessionTag() == tag) {
    return GetSessionWindows(*local_session_);
  }
  if (SyncedSession* session = FindForeignSession(tag)) {
    return GetSessionWindows(*session);
  }
  return {};
}

bool FakeOpenTabsUIDelegate::GetForeignSessionTabs(
    const std::string& tag,
    std::vector<const sessions::SessionTab*>* tabs) {
  NOTIMPLEMENTED();
  return false;
}

bool FakeOpenTabsUIDelegate::GetLocalSession(const SyncedSession** local) {
  *local = local_session_.get();
  return *local != nullptr;
}

SyncedSession* FakeOpenTabsUIDelegate::SetLocalSession(
    std::unique_ptr<SyncedSession> local_session) {
  local_session_ = std::move(local_session);
  return local_session_.get();
}

SyncedSession* FakeOpenTabsUIDelegate::AddForeignSession(
    const std::string& tag,
    base::Time modified_time) {
  auto session = std::make_unique<SyncedSession>();
  session->SetSessionTag(tag);
  session->SetModifiedTime(modified_time);
  return AddForeignSession(std::move(session));
}

SyncedSession* FakeOpenTabsUIDelegate::AddForeignSession(
    std::unique_ptr<SyncedSession> session) {
  SyncedSession* ptr = session.get();
  foreign_sessions_.push_back(std::move(session));
  return ptr;
}

sessions::SessionTab* FakeOpenTabsUIDelegate::AddTabToForeignSession(
    const std::string& session_tag,
    const GURL& url,
    const SessionID& tab_id) {
  SyncedSession* found_session = FindForeignSession(session_tag);
  if (!found_session) {
    found_session = AddForeignSession(session_tag, base::Time::Now());
  }
  auto window = std::make_unique<SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();
  tab->tab_id = tab_id;
  if (url.is_valid()) {
    sessions::SerializedNavigationEntry navigation;
    navigation.set_virtual_url(url);
    tab->navigations.push_back(navigation);
  }
  sessions::SessionTab* tab_ptr = tab.get();
  window->wrapped_window.tabs.push_back(std::move(tab));
  found_session->windows[SessionID::NewUnique()] = std::move(window);
  return tab_ptr;
}

SyncedSession* FakeOpenTabsUIDelegate::FindForeignSession(
    const std::string& tag) {
  for (const auto& session : foreign_sessions_) {
    if (session->GetSessionTag() == tag) {
      return session.get();
    }
  }
  return nullptr;
}

}  // namespace sync_sessions
