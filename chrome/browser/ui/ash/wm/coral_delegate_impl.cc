// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wm/coral_delegate_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/desks/desks_templates_app_launch_handler.h"
#include "chrome/browser/ui/browser.h"
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

constexpr base::TimeDelta kClearLaunchDataDuration = base::Seconds(20);

// Returns the first `AppRestoreData` in `restore_data` associated with
// `app_id`. If one is found, the also `out_window_id` will have the window id
// of the first window associated with that app.
// TODO(http://crbug.com/365839465): The window id should also be passed
// through the pipeline.
app_restore::AppRestoreData* GetFirstAppRestoreData(
    app_restore::RestoreData* restore_data,
    const std::string& app_id,
    int32_t& out_window_id) {
  if (!restore_data) {
    return nullptr;
  }
  auto launch_list_it = restore_data->app_id_to_launch_list().find(app_id);
  if (launch_list_it == restore_data->app_id_to_launch_list().end()) {
    return nullptr;
  }
  const app_restore::RestoreData::LaunchList& launch_list =
      launch_list_it->second;
  if (launch_list.empty()) {
    return nullptr;
  }
  out_window_id = launch_list.begin()->first;
  return launch_list.begin()->second.get();
}

// Converts a coral `group` to a full restore struct that can be used by
// `DesksTemplatesAppLaunchHandler` to create a browser and launch apps.
std::unique_ptr<app_restore::RestoreData> CoralGroupToRestoreData(
    coral::mojom::GroupPtr group,
    Profile* profile) {
  auto restore_data = std::make_unique<app_restore::RestoreData>();
  std::vector<GURL> tab_urls;
  std::vector<std::string> app_ids;
  for (const coral::mojom::EntityPtr& entity : group->entities) {
    if (entity->is_tab()) {
      tab_urls.push_back(entity->get_tab()->url);
    } else if (entity->is_app()) {
      app_ids.push_back(entity->get_app()->id);
    }
  }

  if (!tab_urls.empty()) {
    auto& launch_list =
        restore_data
            ->mutable_app_id_to_launch_list()[app_constants::kChromeAppId];
    // All tabs go into the same window.
    auto& app_restore_data =
        launch_list[/*window_id=*/Browser::kDefaultRestoreId];
    app_restore_data = std::make_unique<app_restore::AppRestoreData>();
    app_restore_data->browser_extra_info.urls = std::move(tab_urls);
  }

  app_restore::RestoreData* full_restore_restore_data =
      ash::full_restore::FullRestoreServiceFactory::GetForProfile(profile)
          ->app_launch_handler()
          ->restore_data();
  for (const std::string& app_id : app_ids) {
    // TODO(http://crbug.com/365839465): The window id should also be passed
    // through the pipeline. For now we just use the first window restore data
    // as multi window apps are rare.
    int32_t window_id;
    app_restore::AppRestoreData* full_restore_app_restore_data =
        GetFirstAppRestoreData(full_restore_restore_data, app_id, window_id);
    if (!full_restore_app_restore_data) {
      // TODO(sammiequon): PWA's need a window id to be identified. For now we
      // will launch apps without full restore data at default positions.
      auto& new_launch_list =
          restore_data->mutable_app_id_to_launch_list()[app_id];
      auto& new_app_restore_data = new_launch_list[/*window_id=*/0];
      new_app_restore_data = std::make_unique<app_restore::AppRestoreData>();
      new_app_restore_data->container = 0;
      new_app_restore_data->display_id =
          display::Screen::GetScreen()->GetPrimaryDisplay().id();
      new_app_restore_data->disposition = 3;
      continue;
    }
    auto& launch_list = restore_data->mutable_app_id_to_launch_list()[app_id];
    auto& app_restore_data = launch_list[window_id];
    app_restore_data = full_restore_app_restore_data->Clone();
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

void CoralDelegateImpl::OnPostLoginLaunchComplete() {
  app_launch_handler_.reset();
}

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
      CoralGroupToRestoreData(std::move(group), active_profile),
      DesksTemplatesAppLaunchHandler::GetNextLaunchId());

  // Clears the launch handler after a given duration.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CoralDelegateImpl::OnPostLoginLaunchComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      kClearLaunchDataDuration);
}

void CoralDelegateImpl::MoveTabsInGroupToNewDesk(
    const std::vector<coral::mojom::Tab>& tabs) {
  Browser* target_browser = nullptr;
  for (const auto& tab : tabs) {
    // Find the index of the tab item on its browser window.
    const auto& tab_url = tab.url;
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
      std::unique_ptr<tabs::TabModel> tab_model =
          source_tab_strip->DetachTabAtForInsertion(tab_index);
      target_browser->tab_strip_model()->InsertDetachedTabAt(
          -1, std::move(tab_model), add_types);
    }
  }

  if (target_browser) {
    target_browser->window()->ShowInactive();
  }
}

void CoralDelegateImpl::CreateSavedDeskFromGroup(coral::mojom::GroupPtr group) {
}
