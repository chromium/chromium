// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wm/coral_delegate_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/desks/desks_templates_app_launch_handler.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "chromeos/ui/wm/desks/desks_helper.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/restore_data.h"
#include "components/user_manager/user_manager.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

std::unique_ptr<app_restore::RestoreData> CoralGroupToRestoreData(
    coral::mojom::GroupPtr group) {
  auto restore_data = std::make_unique<app_restore::RestoreData>();
  std::vector<GURL> tab_urls;
  std::vector<std::string> app_ids;
  for (const coral::mojom::EntityKeyPtr& entity : group->entities) {
    if (entity->is_tab_url()) {
      tab_urls.push_back(entity->get_tab_url());
    } else if (entity->is_app_id()) {
      app_ids.push_back(entity->get_app_id());
    }
  }

  if (!tab_urls.empty()) {
    auto& launch_list =
        restore_data
            ->mutable_app_id_to_launch_list()[app_constants::kChromeAppId];
    // All tabs go into the same window.
    auto& app_restore_data = launch_list[/*window_id=*/0];
    app_restore_data = std::make_unique<app_restore::AppRestoreData>();
    app_restore_data->browser_extra_info.urls = std::move(tab_urls);
  }

  for (const std::string& app_id : app_ids) {
    auto& launch_list = restore_data->mutable_app_id_to_launch_list()[app_id];
    auto& app_restore_data = launch_list[/*window_id=*/0];
    app_restore_data = std::make_unique<app_restore::AppRestoreData>();

    // TODO(http://b/365839465): These fields are required to launch an app.
    // Retrieve them from full restore read handler.
    app_restore_data->container = 0;
    app_restore_data->display_id =
        display::Screen::GetScreen()->GetPrimaryDisplay().id();
    app_restore_data->disposition = 3;
  }

  return restore_data;
}

// Gets profile from the active user.
Profile* GetActiveUserProfile() {
  const auto* active_user = user_manager::UserManager::Get()->GetActiveUser();
  if (!active_user) {
    return nullptr;
  }

  auto* browser_context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(active_user);
  return Profile::FromBrowserContext(browser_context);
}

// Creates a browser on the new desk.
Browser* CreateBrowserOnNewDesk() {
  Profile* active_profile = GetActiveUserProfile();
  if (!active_profile) {
    return nullptr;
  }

  Browser::CreateParams params(Browser::Type::TYPE_NORMAL, active_profile,
                               /*user_gesture=*/false);
  params.should_trigger_session_restore = false;
  params.initial_workspace = base::NumberToString(
      chromeos::DesksHelper::Get(nullptr)->GetNumberOfDesks() - 1);
  return Browser::Create(std::move(params));
}

// Finds the first tab with given url on the active desk and returns the source
// browser and the tab index.
Browser* FindTabOnActiveDesk(const GURL& url, int& out_tab_index) {
  out_tab_index = -1;
  auto* desks_helper = chromeos::DesksHelper::Get(nullptr);
  for (auto browser : *BrowserList::GetInstance()) {
    // Guarantee the window belongs to the active desk.
    if (!desks_helper->BelongsToActiveDesk(
            browser->window()->GetNativeWindow())) {
      continue;
    }

    if (browser->profile()->IsIncognitoProfile()) {
      continue;
    }

    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int idx = 0; idx < tab_strip_model->count(); idx++) {
      if (tab_strip_model->GetWebContentsAt(idx)->GetVisibleURL() == url) {
        out_tab_index = idx;
        return browser;
      }
    }
  }
  return nullptr;
}

}  // namespace

CoralDelegateImpl::CoralDelegateImpl() = default;

CoralDelegateImpl::~CoralDelegateImpl() = default;

void CoralDelegateImpl::LaunchPostLoginGroup(coral::mojom::GroupPtr group) {
  if (app_launch_handler_) {
    return;
  }

  Profile* active_profile = GetActiveUserProfile();
  if (!active_profile) {
    return;
  }

  app_launch_handler_ = std::make_unique<DesksTemplatesAppLaunchHandler>(
      active_profile, DesksTemplatesAppLaunchHandler::Type::kCoral);
  app_launch_handler_->LaunchCoralGroup(
      CoralGroupToRestoreData(std::move(group)),
      DesksTemplatesAppLaunchHandler::GetNextLaunchId());
}

void CoralDelegateImpl::MoveTabsInGroupToNewDesk(coral::mojom::GroupPtr group) {
  Browser* target_browser = nullptr;
  for (const auto& entity : group->entities) {
    if (!entity->is_tab_url()) {
      continue;
    }

    // Find the index of the tab item on its browser window.
    const auto& tab_url = entity->get_tab_url();
    int tab_index = -1;
    Browser* source_browser = FindTabOnActiveDesk(tab_url, tab_index);
    if (source_browser) {
      // Create a browser on the new desk if there is none.
      if (!target_browser) {
        target_browser = CreateBrowserOnNewDesk();
        if (!target_browser) {
          break;
        }
      }

      // Move the tab from source browser to target browser.
      TabStripModel* source_tab_strip = source_browser->tab_strip_model();
      bool was_pinned = source_tab_strip->IsTabPinned(tab_index);
      int add_types =
          was_pinned ? AddTabTypes::ADD_PINNED : AddTabTypes::ADD_ACTIVE;
      std::unique_ptr<tabs::TabModel> tab =
          source_tab_strip->DetachTabAtForInsertion(tab_index);
      target_browser->tab_strip_model()->InsertDetachedTabAt(-1, std::move(tab),
                                                             add_types);
    }
  }

  if (target_browser) {
    target_browser->window()->ShowInactive();
  }
}

void CoralDelegateImpl::CreateSavedDeskFromGroup(coral::mojom::GroupPtr group) {
}
