// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/foreign_tabs_page_handler.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

std::string GetLastActiveElapsedText(const base::Time& last_active_time) {
  const base::TimeDelta elapsed =
      std::max(base::TimeDelta(), base::Time::Now() - last_active_time);
  return base::UTF16ToUTF8(ui::TimeFormat::Simple(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT, elapsed));
}

base::Time GetTabTimestamp(const sessions::SessionTab& tab) {
  if (!tab.timestamp.is_null()) {
    return tab.timestamp;
  }
  if (!tab.navigations.empty()) {
    return tab.navigations.at(tab.normalized_navigation_index()).timestamp();
  }
  return base::Time();
}

}  // namespace

ForeignTabsPageHandler::ForeignTabsPageHandler(
    mojo::PendingReceiver<organizer_panel::mojom::ForeignTabsPageHandler>
        receiver,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
  CHECK(profile_);
}

ForeignTabsPageHandler::~ForeignTabsPageHandler() = default;

void ForeignTabsPageHandler::OpenTab(const std::string& session_tag,
                                     int32_t session_tab_id) {
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_contents_);
  if (!browser) {
    return;
  }
  content::WebContents* source_web_contents =
      browser->GetActiveTabInterface()
          ? browser->GetActiveTabInterface()->GetContents()
          : nullptr;
  if (!source_web_contents) {
    return;
  }
  SessionID tab_id = SessionID::FromSerializedValue(session_tab_id);
  if (!tab_id.is_valid()) {
    return;
  }
  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  if (!session_sync_service) {
    return;
  }
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      session_sync_service->GetOpenTabsUIDelegate();
  if (!open_tabs) {
    return;
  }
  const sessions::SessionTab* session_tab = nullptr;
  if (!open_tabs->GetForeignTab(session_tag, tab_id, &session_tab) ||
      !session_tab || session_tab->navigations.empty()) {
    return;
  }
  SessionRestore::RestoreForeignSessionTab(
      source_web_contents, *session_tab,
      WindowOpenDisposition::kNewForegroundTab);
}

void ForeignTabsPageHandler::GetForeignTabs(GetForeignTabsCallback callback) {
  std::vector<organizer_panel::mojom::ForeignTabPtr> result;

  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  // This will be null if sync of the tab sync datatype are disabled.
  if (!session_sync_service) {
    std::move(callback).Run(std::move(result));
    return;
  }

  sync_sessions::OpenTabsUIDelegate* open_tabs =
      session_sync_service->GetOpenTabsUIDelegate();
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  if (!open_tabs || !open_tabs->GetAllForeignSessions(&sessions)) {
    std::move(callback).Run(std::move(result));
    return;
  }

  struct TabData {
    raw_ptr<const sessions::SessionTab> tab;
    std::string session_tag;
    std::string device_name;
  };

  std::vector<TabData> all_tabs;
  for (const sync_sessions::SyncedSession* session : sessions) {
    const std::string& session_tag = session->GetSessionTag();
    std::vector<const sessions::SessionTab*> tabs_in_session;
    if (!open_tabs->GetForeignSessionTabs(session_tag, &tabs_in_session) ||
        tabs_in_session.empty()) {
      continue;
    }

    for (const sessions::SessionTab* session_tab : tabs_in_session) {
      if (session_tab->navigations.empty()) {
        continue;
      }
      all_tabs.push_back({session_tab, session_tag, session->GetSessionName()});
    }
  }

  std::ranges::stable_sort(all_tabs, std::greater(), [](const TabData& data) {
    return GetTabTimestamp(*data.tab);
  });

  result.reserve(all_tabs.size());
  for (const auto& data : all_tabs) {
    const sessions::SessionTab* session_tab = data.tab;
    const sessions::SerializedNavigationEntry& current_navigation =
        session_tab->navigations.at(session_tab->normalized_navigation_index());

    auto tab_mojom = organizer_panel::mojom::ForeignTab::New();
    tab_mojom->session_tag = data.session_tag;
    tab_mojom->session_tab_id = session_tab->tab_id.id();
    std::string title = base::UTF16ToUTF8(current_navigation.title());
    if (title.empty()) {
      title = current_navigation.virtual_url().spec();
    }
    tab_mojom->title = title;
    tab_mojom->url = current_navigation.virtual_url();
    base::Time timestamp = GetTabTimestamp(*session_tab);
    if (!timestamp.is_null()) {
      tab_mojom->last_active_elapsed_text = GetLastActiveElapsedText(timestamp);
    }
    tab_mojom->device_name = data.device_name;

    result.push_back(std::move(tab_mojom));
  }

  std::move(callback).Run(std::move(result));
}
