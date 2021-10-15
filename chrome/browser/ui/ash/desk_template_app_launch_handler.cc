// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desk_template_app_launch_handler.h"

#include <string>

#include "ash/wm/desks/desks_controller.h"
#include "base/notreached.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/app_restore/desk_template_read_handler.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_info.h"
#include "extensions/common/extension.h"

namespace {

// The restore data owned by this class will clear after being set. This is a
// temporary estimate of how long it takes to launch apps.
constexpr base::TimeDelta kClearRestoreDataDuration = base::Seconds(5);

}  // namespace

DeskTemplateAppLaunchHandler::DeskTemplateAppLaunchHandler(Profile* profile)
    : ash::AppLaunchHandler(profile),
      read_handler_(app_restore::DeskTemplateReadHandler::Get()) {}

DeskTemplateAppLaunchHandler::~DeskTemplateAppLaunchHandler() = default;

void DeskTemplateAppLaunchHandler::SetRestoreDataAndLaunch(
    std::unique_ptr<app_restore::RestoreData> new_restore_data) {
  // Another desk template is underway.
  // TODO(sammiequon): Checking the read handler for restore data is temporary.
  // We will want to use a better check of whether a desk template is underway.
  // Perhaps removing entries from read handler's restore data of launched apps
  // and/or using individual shorter timeouts.
  if (read_handler_->restore_data())
    return;

  set_restore_data(std::move(new_restore_data));

  if (!HasRestoreData())
    return;

  read_handler_->SetRestoreData(restore_data()->Clone());

  LaunchApps();
  LaunchBrowsers();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DeskTemplateAppLaunchHandler::
                         ClearDeskTemplateReadHandlerRestoreData,
                     weak_ptr_factory_.GetWeakPtr()),
      kClearRestoreDataDuration);
}

bool DeskTemplateAppLaunchHandler::ShouldLaunchSystemWebAppOrChromeApp(
    const std::string& app_id,
    const app_restore::RestoreData::LaunchList& launch_list) {
  // Find out if the app can have multiple instances. Apps that can have
  // multiple instances are:
  //   1) System web apps which can open multiple windows
  //   2) Non platform app type Chrome apps
  // TODO(crubg.com/1239089): Investigate if we can have a way to handle moving
  // single instance windows without all these heuristics.

  bool is_multi_instance_window = false;

  // Check the app registry cache to see if the app is a system web app.
  bool is_system_web_app = false;
  apps::AppServiceProxyFactory::GetForProfile(profile())
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
        web_app::GetSystemWebAppTypeForAppId(profile(), app_id);
    if (swa_type.has_value()) {
      auto* system_app = web_app::WebAppProvider::GetForSystemWebApps(profile())
                             ->system_web_app_manager()
                             .GetSystemApp(*swa_type);
      DCHECK(system_app);
      is_multi_instance_window = system_app->ShouldShowNewWindowMenuOption();
    }
  } else {
    // Check the extensions registry to see if the app is a platform app. No
    // need to do this check if the app is a system web app.
    DCHECK(!is_multi_instance_window);
    const extensions::Extension* extension =
        GetExtensionForAppID(app_id, profile());
    is_multi_instance_window = extension && !extension->is_platform_app();
  }

  // Do not try sending an existing window to the active desk and launch a new
  // instance.
  if (is_multi_instance_window)
    return true;

  return ash::DesksController::Get()->OnSingleInstanceAppLaunchingFromTemplate(
      app_id, launch_list);
}

void DeskTemplateAppLaunchHandler::OnExtensionLaunching(
    const std::string& app_id) {
  read_handler_->SetNextRestoreWindowIdForChromeApp(app_id);
}

base::WeakPtr<ash::AppLaunchHandler>
DeskTemplateAppLaunchHandler::GetWeakPtrAppLaunchHandler() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DeskTemplateAppLaunchHandler::LaunchBrowsers() {
  DCHECK(restore_data());

  const auto& launch_list = restore_data()->app_id_to_launch_list();
  for (const auto& iter : launch_list) {
    const std::string& app_id = iter.first;
    if (app_id != extension_misc::kChromeAppId)
      continue;

    for (const auto& window_iter : iter.second) {
      const std::unique_ptr<app_restore::AppRestoreData>& app_restore_data =
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
              ? Browser::CreateParams::CreateForApp(app_name,
                                                    /*trusted_source=*/true,
                                                    current_bounds, profile(),
                                                    /*user_gesture=*/false)
              : Browser::CreateParams(Browser::TYPE_NORMAL, profile(),
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
  restore_data()->RemoveApp(extension_misc::kChromeAppId);
}

void DeskTemplateAppLaunchHandler::ClearDeskTemplateReadHandlerRestoreData() {
  read_handler_->SetRestoreData(nullptr);
}

void DeskTemplateAppLaunchHandler::RecordRestoredAppLaunch(
    apps::AppTypeName app_type_name) {
  // TODO: Add UMA Histogram.
  NOTIMPLEMENTED();
}
