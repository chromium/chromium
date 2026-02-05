// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/fake_open_tabs_ui_delegate.h"

#include <functional>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/notimplemented.h"

namespace sync_sessions {

FakeOpenTabsUIDelegate::FakeOpenTabsUIDelegate() = default;
FakeOpenTabsUIDelegate::~FakeOpenTabsUIDelegate() = default;

bool FakeOpenTabsUIDelegate::GetAllForeignSessions(
    std::vector<raw_ptr<const SyncedSession, VectorExperimental>>* sessions) {
  sessions->assign(foreign_sessions_.begin(), foreign_sessions_.end());
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
  NOTIMPLEMENTED();
  return false;
}

void FakeOpenTabsUIDelegate::DeleteForeignSession(const std::string& tag) {
  NOTIMPLEMENTED();
}

std::vector<const sessions::SessionWindow*>
FakeOpenTabsUIDelegate::GetForeignSession(const std::string& tag) {
  NOTIMPLEMENTED();
  return {};
}

bool FakeOpenTabsUIDelegate::GetForeignSessionTabs(
    const std::string& tag,
    std::vector<const sessions::SessionTab*>* tabs) {
  NOTIMPLEMENTED();
  return false;
}

bool FakeOpenTabsUIDelegate::GetLocalSession(const SyncedSession** local) {
  NOTIMPLEMENTED();
  return false;
}

void FakeOpenTabsUIDelegate::SetForeignSessions(
    const std::vector<raw_ptr<const SyncedSession>>& sessions) {
  foreign_sessions_ = sessions;
}

}  // namespace sync_sessions
