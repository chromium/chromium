// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_item_controller.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/pip/arc_pip_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/app_window_base.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"

AppServiceAppWindowShelfItemController::AppServiceAppWindowShelfItemController(
    const ash::ShelfID& shelf_id,
    AppServiceAppWindowShelfController* controller)
    : AppWindowShelfItemController(shelf_id), controller_(controller) {
  DCHECK(controller_);
}

AppServiceAppWindowShelfItemController::
    ~AppServiceAppWindowShelfItemController() = default;

void AppServiceAppWindowShelfItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback,
    const ItemFilterPredicate& filter_predicate) {
  if (window_count()) {
    // Tapping the shelf icon of an app that's showing PIP means expanding PIP.
    // Even if the app contains multiple windows, we just expand PIP without
    // showing the menu on the shelf icon.
    for (AppWindowBase* window : windows()) {
      aura::Window* native_window = window->GetNativeWindow();
      if (native_window->GetProperty(chromeos::kWindowStateTypeKey) ==
          chromeos::WindowStateType::kPip) {
        Profile* profile = ChromeShelfController::instance()->profile();
        arc::ArcPipBridge* pip_bridge =
            arc::ArcPipBridge::GetForBrowserContext(profile);
        if (pip_bridge) {
          // ClosePip() actually expands PIP.
          pip_bridge->ClosePip();
          std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
          return;
        }
      }
    }
    AppWindowShelfItemController::ItemSelected(std::move(event), display_id,
                                               source, std::move(callback),
                                               filter_predicate);
    return;
  }

  if (!task_ids_.empty()) {
    arc::SetTaskActive(*task_ids_.begin());
    std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
    return;
  }

  if (session_ids_.empty()) {
    NOTREACHED_IN_MIGRATION();
  }

  std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
}

ash::ShelfItemDelegate::AppMenuItems
AppServiceAppWindowShelfItemController::GetAppMenuItems(
    int event_flags,
    const ItemFilterPredicate& filter_predicate) {
  if (!IsChromeApp()) {
    return AppWindowShelfItemController::GetAppMenuItems(event_flags,
                                                         filter_predicate);
  }

  // The window could be teleported from the inactive user's profile to the
  // current active user, so search all profiles.
  for (Profile* profile : controller_->GetProfileList()) {
    extensions::AppWindowRegistry* const app_window_registry =
        extensions::AppWindowRegistry::Get(profile);
    DCHECK(app_window_registry);

    AppMenuItems items;
    bool switch_profile = false;
    int command_id = -1;
    for (const ui::BaseWindow* window : windows()) {
      ++command_id;
      auto* native_window = window->GetNativeWindow();
      if (!filter_predicate.is_null() && !filter_predicate.Run(native_window))
        continue;

      extensions::AppWindow* const app_window =
          app_window_registry->GetAppWindowForNativeWindow(native_window);
      if (!app_window) {
        switch_profile = true;
        break;
      }

      // Use the app's web contents favicon, or the app window's icon.
      favicon::FaviconDriver* const favicon_driver =
          favicon::ContentFaviconDriver::FromWebContents(
              app_window->web_contents());
      DCHECK(favicon_driver);
      gfx::ImageSkia image = favicon_driver->GetFavicon().AsImageSkia();
      if (image.isNull()) {
        const gfx::ImageSkia* app_icon = nullptr;
        if (app_window->GetNativeWindow()) {
          app_icon = app_window->GetNativeWindow()->GetProperty(
              aura::client::kAppIconKey);
        }
        if (app_icon && !app_icon->isNull())
          image = *app_icon;
      }

      items.push_back({command_id, app_window->GetTitle(), image});
    }
    if (!switch_profile)
      return items;
  }
  return AppMenuItems();
}

void AppServiceAppWindowShelfItemController::OnWindowTitleChanged(
    aura::Window* window) {
  if (!IsChromeApp())
    return;

  ui::BaseWindow* const base_window =
      GetAppWindow(window, true /*include_hidden*/);

  // For Chrome apps, use the window title (if set) to differentiate
  // show_in_shelf window shelf items instead of the default behavior of using
  // the app name.
  //
  // The window could be teleported from the inactive user's profile to the
  // current active user, so search all profiles.
  for (Profile* profile : controller_->GetProfileList()) {
    extensions::AppWindowRegistry* const app_window_registry =
        extensions::AppWindowRegistry::Get(profile);
    DCHECK(app_window_registry);

    extensions::AppWindow* const app_window =
        app_window_registry->GetAppWindowForNativeWindow(
            base_window->GetNativeWindow());
    if (!app_window)
      continue;

    if (app_window->show_in_shelf()) {
      const std::u16string title = window->GetTitle();
      if (!title.empty())
        ChromeShelfController::instance()->SetItemTitle(shelf_id(), title);
    }
    return;
  }
}

void AppServiceAppWindowShelfItemController::AddTaskId(int task_id) {
  task_ids_.insert(task_id);
}

void AppServiceAppWindowShelfItemController::RemoveTaskId(int task_id) {
  task_ids_.erase(task_id);
}

bool AppServiceAppWindowShelfItemController::HasAnyTasks() const {
  return !task_ids_.empty();
}

void AppServiceAppWindowShelfItemController::AddSessionId(int session_id) {
  session_ids_.insert(session_id);
}

void AppServiceAppWindowShelfItemController::RemoveSessionId(int session_id) {
  session_ids_.erase(session_id);
}

bool AppServiceAppWindowShelfItemController::HasAnySessions() const {
  return !session_ids_.empty();
}

bool AppServiceAppWindowShelfItemController::IsChromeApp() {
  Profile* const profile = ChromeShelfController::instance()->profile();
  return apps::AppServiceProxyFactory::GetForProfile(profile)
             ->AppRegistryCache()
             .GetAppType(shelf_id().app_id) == apps::AppType::kChromeApp;
}
