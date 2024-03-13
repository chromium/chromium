// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_recent_tabs_provider.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/suggestion_service_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"

namespace ash {

namespace {

BirchTabItem::DeviceFormFactor GetTabItemFormFactor(
    syncer::DeviceInfo::FormFactor form_factor) {
  // Convert to a BirchTabItem specific form factor to ensure any changes
  // to DeviceInfo::FormFactor won't break the BirchTabItem's form factor.
  switch (form_factor) {
    case syncer::DeviceInfo::FormFactor::kUnknown:
    case syncer::DeviceInfo::FormFactor::kDesktop:
      return BirchTabItem::DeviceFormFactor::kDesktop;
    case syncer::DeviceInfo::FormFactor::kPhone:
      return BirchTabItem::DeviceFormFactor::kPhone;
    case syncer::DeviceInfo::FormFactor::kTablet:
      return BirchTabItem::DeviceFormFactor::kTablet;
  }
}

BirchTabItem::DeviceFormFactor FromMojomFormFactor(
    crosapi::mojom::SuggestionDeviceFormFactor form_factor) {
  switch (form_factor) {
    case crosapi::mojom::SuggestionDeviceFormFactor::kDesktop:
      return BirchTabItem::DeviceFormFactor::kDesktop;
    case crosapi::mojom::SuggestionDeviceFormFactor::kPhone:
      return BirchTabItem::DeviceFormFactor::kPhone;
    case crosapi::mojom::SuggestionDeviceFormFactor::kTablet:
      return BirchTabItem::DeviceFormFactor::kTablet;
  }
}

}  // namespace

BirchRecentTabsProvider::BirchRecentTabsProvider(Profile* profile)
    : profile_(profile) {}

BirchRecentTabsProvider::~BirchRecentTabsProvider() = default;

void BirchRecentTabsProvider::RequestBirchDataFetch() {
  if (crosapi::browser_util::IsLacrosEnabled()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->suggestion_service_ash()
        ->GetTabSuggestionItems(base::BindOnce(
            &BirchRecentTabsProvider::OnTabsRetrieved, base::Unretained(this)));
    return;
  }

  auto* session_sync_service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);

  sync_sessions::OpenTabsUIDelegate* open_tabs =
      session_sync_service->GetOpenTabsUIDelegate();
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      remote_sessions;

  if (!open_tabs || !open_tabs->GetAllForeignSessions(&remote_sessions)) {
    Shell::Get()->birch_model()->SetRecentTabItems({});
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
            session->GetSessionName(),
            GetTabItemFormFactor(session->GetDeviceFormFactor()));
      }
    }
  }

  Shell::Get()->birch_model()->SetRecentTabItems(std::move(items));
}

void BirchRecentTabsProvider::OnTabsRetrieved(
    std::vector<crosapi::mojom::TabSuggestionItemPtr> items) {
  std::vector<BirchTabItem> tab_items;
  for (auto& item : items) {
    tab_items.emplace_back(base::UTF8ToUTF16(item->title), item->url,
                           item->timestamp, item->favicon_url,
                           item->session_name,
                           FromMojomFormFactor(item->form_factor));
  }
  Shell::Get()->birch_model()->SetRecentTabItems(std::move(tab_items));
}

}  // namespace ash
