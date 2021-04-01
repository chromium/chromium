// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_launcher_item_controller.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/pip/arc_pip_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"

AppServiceAppWindowLauncherItemController::
    AppServiceAppWindowLauncherItemController(
        const ash::ShelfID& shelf_id,
        AppServiceAppWindowLauncherController* controller)
    : AppWindowLauncherItemController(shelf_id), controller_(controller) {
  DCHECK(controller_);
}

AppServiceAppWindowLauncherItemController::
    ~AppServiceAppWindowLauncherItemController() {}

void AppServiceAppWindowLauncherItemController::ItemSelected(
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
        Profile* profile = ChromeLauncherController::instance()->profile();
        arc::ArcPipBridge* pip_bridge =
            arc::ArcPipBridge::GetForBrowserContext(profile);
        // ClosePip() actually expands PIP.
        pip_bridge->ClosePip();
        std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
        return;
      }
    }
    AppWindowLauncherItemController::ItemSelected(std::move(event), display_id,
                                                  source, std::move(callback),
                                                  filter_predicate);
    return;
  }

  if (task_ids_.empty()) {
    NOTREACHED();
    std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
    return;
  }
  arc::SetTaskActive(*task_ids_.begin());
  std::move(callback).Run(ash::SHELF_ACTION_NEW_WINDOW_CREATED, {});
}

ash::ShelfItemDelegate::AppMenuItems
AppServiceAppWindowLauncherItemController::GetAppMenuItems(
    int event_flags,
    const ItemFilterPredicate& filter_predicate) {
  if (!IsChromeApp()) {
    return AppWindowLauncherItemController::GetAppMenuItems(event_flags,
                                                            filter_predicate);
  }

  // The window could be teleported from the inactive user's profile to the
  // current active user, so search all profiles.
  for (auto* profile : controller_->GetProfileList()) {
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

void AppServiceAppWindowLauncherItemController::OnWindowTitleChanged(
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
  for (auto* profile : controller_->GetProfileList()) {
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
        ChromeLauncherController::instance()->SetItemTitle(shelf_id(), title);
    }
    return;
  }
}

void AppServiceAppWindowLauncherItemController::AddTaskId(int task_id) {
  task_ids_.insert(task_id);
}

void AppServiceAppWindowLauncherItemController::RemoveTaskId(int task_id) {
  task_ids_.erase(task_id);
}

bool AppServiceAppWindowLauncherItemController::HasAnyTasks() const {
  return !task_ids_.empty();
}

bool AppServiceAppWindowLauncherItemController::IsChromeApp() {
  Profile* const profile = ChromeLauncherController::instance()->profile();
  return apps::AppServiceProxyFactory::GetForProfile(profile)
             ->AppRegistryCache()
             .GetAppType(shelf_id().app_id) == apps::mojom::AppType::kExtension;
}
