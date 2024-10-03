// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks/desks_templates_app_launch_handler.h"

#include <string>

#include "ash/public/cpp/desk_template.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_restore/app_launch_handler.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"
#include "chrome/browser/ash/app_restore/arc_app_queue_restore_handler.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/desks/chrome_desks_util.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/desk_template_read_handler.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_info.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/tab_groups/tab_group_info.h"
#include "extensions/common/extension.h"

namespace {

// Used to generate unique IDs for desk launches.
int32_t g_launch_id = 0;

// Returns the browser app name if it's an app type browser. Returns an empty
// string otherwise.
std::string GetBrowserAppName(
    const std::unique_ptr<app_restore::AppRestoreData>& app_restore_data,
    const std::string& app_id) {
  const bool app_type_browser =
      app_restore_data->browser_extra_info.app_type_browser.value_or(false);
  if (!app_type_browser)
    return std::string();

  const std::optional<std::string>& maybe_app_name =
      app_restore_data->browser_extra_info.app_name;
  return maybe_app_name.has_value() && !maybe_app_name.value().empty()
             ? maybe_app_name.value()
             : app_id;
}

}  // namespace

DesksTemplatesAppLaunchHandler::DesksTemplatesAppLaunchHandler(Profile* profile,
                                                               Type type)
    : ash::AppLaunchHandler(profile),
      type_(type),
      read_handler_(app_restore::DeskTemplateReadHandler::Get()) {}

DesksTemplatesAppLaunchHandler::~DesksTemplatesAppLaunchHandler() {
  if (launch_id_) {
    read_handler_->ClearRestoreData(launch_id_);

    if (auto* arc_task_handler =
            ash::app_restore::AppRestoreArcTaskHandlerFactory::GetForProfile(
                profile())) {
      arc_task_handler->ClearDeskTemplateArcAppQueueRestoreHandler(launch_id_);
    }
  }
}

// static
int32_t DesksTemplatesAppLaunchHandler::GetNextLaunchId() {
  return ++g_launch_id;
}

void DesksTemplatesAppLaunchHandler::LaunchTemplate(
    const ash::DeskTemplate& desk_template,
    int32_t launch_id) {
  // Ensure that the handler isn't re-used.
  DCHECK_EQ(launch_id_, 0);
  launch_id_ = launch_id;

  DCHECK(desk_template.desk_restore_data());
  auto restore_data = desk_template.desk_restore_data()->Clone();
  DCHECK(!restore_data->app_id_to_launch_list().empty());

  read_handler_->SetRestoreData(launch_id_, restore_data->Clone());
  set_restore_data(std::move(restore_data));

  // Launch the different types of apps. They can be done in any order.
  MaybeLaunchArcApps();
  MaybeLaunchLacrosBrowsers();
  LaunchApps();
  LaunchBrowsers();
}

void DesksTemplatesAppLaunchHandler::LaunchCoralGroup(
    std::unique_ptr<app_restore::RestoreData> restore_data,
    int32_t launch_id) {
  // Ensure that the handler isn't re-used.
  CHECK_EQ(launch_id_, 0);
  launch_id_ = launch_id;

  read_handler_->SetRestoreData(launch_id_, restore_data->Clone());
  set_restore_data(std::move(restore_data));

  LaunchBrowsers();
  LaunchApps();
}

void DesksTemplatesAppLaunchHandler::RecordRestoredAppLaunch(
    apps::AppTypeName app_type_name) {
  // TODO: Add UMA Histogram.
  NOTIMPLEMENTED();
}

bool DesksTemplatesAppLaunchHandler::ShouldLaunchSystemWebAppOrChromeApp(
    const std::string& app_id,
    const app_restore::RestoreData::LaunchList& launch_list) {
  // Launched coral groups are intended to be done in the post-login screen. At
  // this point, the assumption is that there are no apps, so we should always
  // launch.
  if (type_ == Type::kCoral) {
    return true;
  }

  // Find out if the app can have multiple instances. Apps that can have
  // multiple instances are:
  //   1) System web apps which can open multiple windows
  //   2) Non platform app type Chrome apps
  // TODO(crbug.com/1239089): Investigate if we can have a way to handle moving
  // single instance windows without all these heuristics.

  bool is_multi_instance_window = false;

  // Check the app registry cache to see if the app is a system web app.
  bool is_system_web_app = false;
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [&is_system_web_app](const apps::AppUpdate& update) {
        if (update.AppType() == apps::AppType::kWeb ||
            update.AppType() == apps::AppType::kSystemWeb) {
          is_system_web_app = true;
        }
      });

  // A SWA can handle multiple instances if it can open multiple windows.
  if (is_system_web_app) {
    std::optional<ash::SystemWebAppType> swa_type =
        ash::GetSystemWebAppTypeForAppId(profile(), app_id);
    if (swa_type.has_value()) {
      auto* swa_manager = ash::SystemWebAppManager::Get(profile());
      DCHECK(swa_manager);
      auto* system_app = swa_manager->GetSystemApp(*swa_type);
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

  const bool should_launch =
      ash::DesksController::Get()->OnSingleInstanceAppLaunchingFromSavedDesk(
          app_id, launch_list);

  // Notify performance tracker that some tracked windows will be moving.
  if (!should_launch) {
    for (const auto& window : launch_list)
      NotifyMovedSingleInstanceApp(window.first);
  }

  return should_launch;
}

void DesksTemplatesAppLaunchHandler::OnExtensionLaunching(
    const std::string& app_id) {
  read_handler_->SetNextRestoreWindowIdForChromeApp(app_id);
}

base::WeakPtr<ash::AppLaunchHandler>
DesksTemplatesAppLaunchHandler::GetWeakPtrAppLaunchHandler() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DesksTemplatesAppLaunchHandler::LaunchBrowsers() {
  DCHECK(restore_data());

  const auto& launch_list = restore_data()->app_id_to_launch_list();
  for (const auto& iter : launch_list) {
    const std::string& app_id = iter.first;
    if (app_id != app_constants::kChromeAppId) {
      continue;
    }
    for (const auto& window_iter : iter.second) {
      const std::unique_ptr<app_restore::AppRestoreData>& app_restore_data =
          window_iter.second;

      const app_restore::BrowserExtraInfo browser_extra_info =
          app_restore_data->browser_extra_info;
      const std::vector<GURL>& urls = browser_extra_info.urls;
      if (urls.empty()) {
        continue;
      }

      const gfx::Rect current_bounds =
          app_restore_data->window_info.current_bounds.value_or(gfx::Rect());
      const std::string app_name = GetBrowserAppName(app_restore_data, app_id);
      if (!app_name.empty() && !IsBrowserAppInstalled(app_name)) {
        continue;
      }

      Browser::CreateParams create_params =
          !app_name.empty()
              ? Browser::CreateParams::CreateForApp(app_name,
                                                    /*trusted_source=*/true,
                                                    current_bounds, profile(),
                                                    /*user_gesture=*/false)
              : Browser::CreateParams(Browser::TYPE_NORMAL, profile(),
                                      /*user_gesture=*/false);

      create_params.restore_id = window_iter.first;
      create_params.creation_source = Browser::CreationSource::kDeskTemplate;

      const std::optional<chromeos::WindowStateType>& window_state_type =
          app_restore_data->window_info.window_state_type;
      if (window_state_type) {
        create_params.initial_show_state =
            chromeos::ToWindowShowState(*window_state_type);
      }

      if (!current_bounds.IsEmpty())
        create_params.initial_bounds = current_bounds;

      Browser* browser = Browser::Create(create_params);

      std::optional<int32_t> active_tab_index =
          browser_extra_info.active_tab_index;
      for (size_t i = 0; i < urls.size(); i++) {
        chrome::AddTabAt(browser, urls[i], /*index=*/-1,
                         /*foreground=*/
                         (active_tab_index &&
                          base::checked_cast<int32_t>(i) == *active_tab_index));
      }

      if (!browser_extra_info.tab_group_infos.empty()) {
        chrome_desks_util::AttachTabGroupsToBrowserInstance(
            browser_extra_info.tab_group_infos, browser);
      }

      if (browser_extra_info.first_non_pinned_tab_index.has_value() &&
          browser_extra_info.first_non_pinned_tab_index.value() <=
              static_cast<int>(urls.size())) {
        chrome_desks_util::SetBrowserPinnedTabs(
            browser_extra_info.first_non_pinned_tab_index.value(), browser);
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
  restore_data()->RemoveApp(app_constants::kChromeAppId);
}

void DesksTemplatesAppLaunchHandler::MaybeLaunchArcApps() {
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->AppRegistryCache();

  const auto& app_id_to_launch_list = restore_data()->app_id_to_launch_list();

  // Add the ready ARC apps in `app_id_to_launch_list` to `app_ids`.
  std::set<std::string> app_ids;
  cache.ForEachApp(
      [&app_ids, &app_id_to_launch_list](const apps::AppUpdate& update) {
        if (update.Readiness() == apps::Readiness::kReady &&
            update.AppType() == apps::AppType::kArc &&
            base::Contains(app_id_to_launch_list, update.AppId())) {
          app_ids.insert(update.AppId());
        }
      });

  // For each ARC app, check and see if there is an existing instance. We will
  // move this instance over instead of launching a new one. Remove the app
  // from the restore data if it was successfully moved so that the ARC launch
  // handler does not try to launch it later.
  if (type_ == Type::kTemplate) {
    for (const std::string& app_id : app_ids) {
      auto it = app_id_to_launch_list.find(app_id);
      DCHECK(it != app_id_to_launch_list.end());
      if (!ash::DesksController::Get()
               ->OnSingleInstanceAppLaunchingFromSavedDesk(app_id,
                                                           it->second)) {
        for (auto& window : it->second) {
          NotifyMovedSingleInstanceApp(window.first);
        }
        restore_data()->RemoveApp(app_id);
      }
    }
  }

  auto* arc_task_handler =
      ash::app_restore::AppRestoreArcTaskHandlerFactory::GetForProfile(
          profile());
  if (!arc_task_handler)
    return;

  if (auto* launch_handler =
          arc_task_handler->GetDeskTemplateArcAppQueueRestoreHandler(
              launch_id_)) {
    launch_handler->set_desk_template_launch_id(launch_id_);
    launch_handler->RestoreArcApps(this);
  }
}

void DesksTemplatesAppLaunchHandler::MaybeLaunchLacrosBrowsers() {
  DCHECK(restore_data());

  const auto& launch_list = restore_data()->app_id_to_launch_list();
  for (const auto& iter : launch_list) {
    const std::string& app_id = iter.first;
    if (app_id != app_constants::kLacrosAppId)
      continue;

    // Count the number of lacros windows ash intends to launch. Will be
    // checked at lacros side to see if anything is missing between ash and
    // lacros when restoring saved desk.
    // TODO(crbug.com/40910343): Remove after issue is root caused.
    int windows_count = 0;

    for (const auto& [restore_window_id, app_restore_data] : iter.second) {
      const app_restore::BrowserExtraInfo& browser_extra_info =
          app_restore_data->browser_extra_info;
      if (browser_extra_info.urls.empty()) {
        continue;
      }
      const std::string app_name = GetBrowserAppName(app_restore_data, app_id);
      if (!app_name.empty() && !IsBrowserAppInstalled(app_name)) {
        continue;
      }

      // TODO(crbug.com/40910343): Remove after issue is root caused.
      windows_count++;
      LOG(ERROR) << "window " << restore_window_id << " launched by Ash with "
                 << app_restore_data->browser_extra_info.urls.size() << " tabs";

      crosapi::BrowserManager::Get()->CreateBrowserWithRestoredData(
          browser_extra_info.urls,
          app_restore_data->window_info.current_bounds.value_or(gfx::Rect()),
          browser_extra_info.tab_group_infos,
          chromeos::ToWindowShowState(
              app_restore_data->window_info.window_state_type.value_or(
                  chromeos::WindowStateType::kDefault)),
          browser_extra_info.active_tab_index.value_or(0),
          // Values of 0 will be ignored, other type constraints are
          // enforced on the browser side.
          browser_extra_info.first_non_pinned_tab_index.value_or(0), app_name,
          restore_window_id, browser_extra_info.lacros_profile_id.value_or(0));
    }
    // TODO(crbug.com/40910343): Remove after issue is root caused.
    LOG(ERROR) << windows_count
               << " windows launched by Ash in total for this desk";
  }
  restore_data()->RemoveApp(app_constants::kLacrosAppId);
}

void DesksTemplatesAppLaunchHandler::NotifyMovedSingleInstanceApp(
    int32_t window_id) {
  CHECK_EQ(Type::kTemplate, type_);
  DesksClient::Get()->NotifyMovedSingleInstanceApp(window_id);
}

bool DesksTemplatesAppLaunchHandler::IsBrowserAppInstalled(
    const std::string& app_name) {
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->AppRegistryCache();

  std::string app_id = app_restore::GetAppIdFromAppName(app_name);
  bool result = false;
  cache.ForOneApp(app_id, [&result](const apps::AppUpdate& update) {
    if (apps_util::IsInstalled(update.Readiness()) &&
        update.AppType() != apps::AppType::kUnknown) {
      result = true;
    }
  });
  return result;
}
