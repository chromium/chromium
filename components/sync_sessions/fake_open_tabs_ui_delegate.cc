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
  for (const auto& session : foreign_sessions_) {
    sessions->push_back(session.get());
  }
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

SyncedSession* FakeOpenTabsUIDelegate::AddForeignSession(
    const std::string& tag,
    base::Time modified_time) {
  auto session = std::make_unique<SyncedSession>();
  session->SetSessionTag(tag);
  session->SetModifiedTime(modified_time);
  SyncedSession* ptr = session.get();
  foreign_sessions_.push_back(std::move(session));
  return ptr;
}

}  // namespace sync_sessions
