// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desk_template_app_launch_handler.h"

#include <string>

#include "ash/public/cpp/desks_helper.h"
#include "base/notreached.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/full_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/full_restore/full_restore_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/full_restore/restore_data.h"
#include "components/full_restore/window_info.h"
#include "extensions/common/extension.h"

namespace {

// The restore data owned by this class will clear after being set. This is a
// temporary estimate of how long it takes to launch apps.
constexpr base::TimeDelta kClearRestoreDataDuration =
    base::TimeDelta::FromSeconds(5);

}  // namespace

DeskTemplateAppLaunchHandler::DeskTemplateAppLaunchHandler(Profile* profile)
    : ash::AppLaunchHandler(profile) {
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
  ash::full_restore::FullRestoreService* full_restore_service =
      ash::full_restore::FullRestoreService::GetForProfile(profile_);
  if (!full_restore_service)
    return false;
  ash::full_restore::FullRestoreAppLaunchHandler*
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

bool DeskTemplateAppLaunchHandler::ShouldLaunchSystemWebAppOrChromeApp(
    const std::string& app_id) {
  // Find out if the app can have multiple instances. Apps that can have
  // multiple instances are:
  //   1) System web apps which can open multiple windows
  //   2) Non platform app type Chrome apps
  // TODO(crubg.com/1239089): Investigate if we can have a way to handle moving
  // single instance windows without all these heuristics.

  bool is_multi_instance_window = false;

  // Check the app registry cache to see if the app is a system web app.
  bool is_system_web_app = false;
  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&is_system_web_app](const apps::AppUpdate& update) {
        if (update.AppType() == apps::mojom::AppType::kWeb ||
            update.AppType() == apps::mojom::AppType::kSystemWeb) {
          is_system_web_app = true;
        }
      });

  // A SWA can handle multiple instances if it can open multiple windows.
  if (is_system_web_app) {
    absl::optional<web_app::SystemAppType> swa_type =
        web_app::GetSystemWebAppTypeForAppId(profile_, app_id);
    is_multi_instance_window =
        swa_type && web_app::WebAppProvider::GetForSystemWebApps(profile_)
                        ->system_web_app_manager()
                        .ShouldShowNewWindowMenuOption(*swa_type);
  } else {
    // Check the extensions registry to see if the app is a platform app. No
    // need to do this check if the app is a system web app.
    DCHECK(!is_multi_instance_window);
    const extensions::Extension* extension =
        GetExtensionForAppID(app_id, profile_);
    is_multi_instance_window = extension && !extension->is_platform_app();
  }

  // Do not try sending an existing window to the active desk and launch a new
  // instance.
  if (is_multi_instance_window)
    return true;

  return ash::DesksHelper::Get()->OnSingleInstanceAppLaunchingFromTemplate(
      app_id);
}

void DeskTemplateAppLaunchHandler::OnExtensionLaunching(
    const std::string& app_id) {
  if (restore_data_clone_)
    restore_data_clone_->SetNextRestoreWindowIdForChromeApp(app_id);
}

base::WeakPtr<ash::AppLaunchHandler>
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
