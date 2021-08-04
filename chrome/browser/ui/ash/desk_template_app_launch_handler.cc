// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desk_template_app_launch_handler.h"

#include <string>

#include "base/notreached.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/full_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/chromeos/full_restore/full_restore_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/full_restore/restore_data.h"
#include "components/full_restore/window_info.h"
#include "extensions/common/constants.h"

namespace {

// The restore data owned by this class will clear after being set. This is a
// temporary estimate of how long it takes to launch apps.
constexpr base::TimeDelta kClearRestoreDataDuration =
    base::TimeDelta::FromSeconds(5);

}  // namespace

DeskTemplateAppLaunchHandler::DeskTemplateAppLaunchHandler(Profile* profile)
    : chromeos::AppLaunchHandler(profile) {
  full_restore::DeskTemplateReadHandler::GetInstance()->SetDelegate(this);
}

DeskTemplateAppLaunchHandler::~DeskTemplateAppLaunchHandler() {
  full_restore::DeskTemplateReadHandler::GetInstance()->SetDelegate(nullptr);
}

void DeskTemplateAppLaunchHandler::SetRestoreDataAndLaunch(
    std::unique_ptr<full_restore::RestoreData> restore_data) {
  // Another desk template is underway.
  // TODO(sammiequon): Checking for `restore_data_clone_` is temporary. We will
  // want to use a better check of whether a desk template is underway. Perhaps
  // removing entries from `restore_data_clone_` of launched apps and/or using
  // individual shorter timeouts.
  if (restore_data_clone_)
    return;

  restore_data_ = std::move(restore_data);

  if (!HasRestoreData())
    return;

  restore_data_clone_ = restore_data_->Clone();

  LaunchApps();
  LaunchBrowsers();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DeskTemplateAppLaunchHandler::ClearRestoreDataClone,
                     weak_ptr_factory_.GetWeakPtr()),
      kClearRestoreDataDuration);
}

std::unique_ptr<full_restore::WindowInfo>
DeskTemplateAppLaunchHandler::GetWindowInfo(int restore_window_id) {
  if (!restore_data_clone_)
    return nullptr;

  // Try to find the window info associated with `restore_window_id`.
  const full_restore::RestoreData::AppIdToLaunchList& launch_list =
      restore_data_clone_->app_id_to_launch_list();
  for (const auto& it : launch_list) {
    const std::string& app_id = it.first;
    const full_restore::AppRestoreData* app_restore_data =
        restore_data_clone_->GetAppRestoreData(app_id, restore_window_id);
    if (app_restore_data)
      return app_restore_data->GetWindowInfo();
  }

  return nullptr;
}

int32_t DeskTemplateAppLaunchHandler::FetchRestoreWindowId(
    const std::string& app_id) {
  return restore_data_clone_ ? restore_data_clone_->FetchRestoreWindowId(app_id)
                             : 0;
}

bool DeskTemplateAppLaunchHandler::IsFullRestoreRunning() const {
  chromeos::full_restore::FullRestoreService* full_restore_service =
      chromeos::full_restore::FullRestoreService::GetForProfile(profile_);
  if (!full_restore_service)
    return false;
  chromeos::full_restore::FullRestoreAppLaunchHandler*
      full_restore_app_launch_handler =
          full_restore_service->app_launch_handler();
  DCHECK(full_restore_app_launch_handler);
  base::TimeTicks full_restore_start_time =
      full_restore_app_launch_handler->restore_start_time();

  // Full restore has not started yet.
  if (full_restore_start_time.is_null())
    return false;

  // We estimate that full restore is still running if it has been less than
  // five seconds since it started.
  return (base::TimeTicks::Now() - full_restore_start_time) <
         kClearRestoreDataDuration;
}

void DeskTemplateAppLaunchHandler::OnExtensionLaunching(
    const std::string& app_id) {
  if (restore_data_clone_)
    restore_data_clone_->SetNextRestoreWindowIdForChromeApp(app_id);
}

base::WeakPtr<chromeos::AppLaunchHandler>
DeskTemplateAppLaunchHandler::GetWeakPtrAppLaunchHandler() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DeskTemplateAppLaunchHandler::LaunchBrowsers() {
  DCHECK(restore_data_);

  const auto& launch_list = restore_data_->app_id_to_launch_list();
  for (const auto& iter : launch_list) {
    const std::string& app_id = iter.first;
    if (app_id != extension_misc::kChromeAppId)
      continue;

    for (const auto& window_iter : iter.second) {
      const std::unique_ptr<full_restore::AppRestoreData>& app_restore_data =
          window_iter.second;

      absl::optional<std::vector<GURL>> urls = app_restore_data->urls;
      if (!urls || urls->empty())
        continue;

      const bool app_type_browser =
          app_restore_data->app_type_browser.value_or(false);
      const std::string app_name = app_restore_data->app_name.value_or(app_id);
      const gfx::Rect current_bounds =
          app_restore_data->current_bounds.value_or(gfx::Rect());

      Browser::CreateParams create_params =
          app_type_browser
              ? Browser::CreateParams::CreateForApp(
                    app_name, /*trusted_source=*/true, current_bounds, profile_,
                    /*user_gesture=*/false)
              : Browser::CreateParams(Browser::TYPE_NORMAL, profile_,
                                      /*user_gesture=*/false);

      create_params.restore_id = window_iter.first;

      absl::optional<chromeos::WindowStateType> window_state_type(
          app_restore_data->window_state_type);
      if (window_state_type) {
        create_params.initial_show_state =
            chromeos::ToWindowShowState(*window_state_type);
      }

      if (!current_bounds.IsEmpty())
        create_params.initial_bounds = current_bounds;

      Browser* browser = Browser::Create(create_params);

      absl::optional<int32_t> active_tab_index =
          app_restore_data->active_tab_index;
      for (int i = 0; i < urls->size(); i++) {
        chrome::AddTabAt(
            browser, urls->at(i), /*index=*/-1,
            /*foreground=*/(active_tab_index && i == *active_tab_index));
      }

      // We need to handle minimized windows separately since unlike other
      // window types, it's not shown.
      if (window_state_type &&
          *window_state_type == chromeos::WindowStateType::kMinimized) {
        browser->window()->Minimize();
        continue;
      }

      browser->window()->ShowInactive();
    }
  }
  restore_data_->RemoveApp(extension_misc::kChromeAppId);
}

void DeskTemplateAppLaunchHandler::RecordRestoredAppLaunch(
    apps::AppTypeName app_type_name) {
  // TODO: Add UMA Histogram.
  NOTIMPLEMENTED();
}

void DeskTemplateAppLaunchHandler::ClearRestoreDataClone() {
  restore_data_clone_.reset();
}
