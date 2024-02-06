// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_recent_tabs_provider.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"

namespace ash {

BirchRecentTabsProvider::BirchRecentTabsProvider(Profile* profile)
    : profile_(profile) {}

BirchRecentTabsProvider::~BirchRecentTabsProvider() = default;

void BirchRecentTabsProvider::GetRecentTabs() {
  auto* session_sync_service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);

  sync_sessions::OpenTabsUIDelegate* open_tabs =
      session_sync_service->GetOpenTabsUIDelegate();
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      remote_sessions;

  if (!open_tabs || !open_tabs->GetAllForeignSessions(&remote_sessions)) {
    return;
  }

  std::vector<BirchTabItem> items;

  for (auto& session : remote_sessions) {
    const std::string& session_tag = session->GetSessionTag();
    std::vector<const sessions::SessionTab*> tabs_in_session;
    if (open_tabs->GetForeignSessionTabs(session_tag, &tabs_in_session) &&
        !tabs_in_session.empty()) {
      for (auto* tab : tabs_in_session) {
        const sessions::SerializedNavigationEntry& current_navigation =
            tab->navigations.at(tab->normalized_navigation_index());
        items.emplace_back(
            current_navigation.title(), current_navigation.virtual_url(),
            current_navigation.timestamp(), current_navigation.favicon_url(),
            session->GetSessionName());
      }
    }
  }

  Shell::Get()->birch_model()->SetRecentTabItems(std::move(items));
}

}  // namespace ash
