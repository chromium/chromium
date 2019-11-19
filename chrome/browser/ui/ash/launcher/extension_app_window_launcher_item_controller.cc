// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/extension_app_window_launcher_item_controller.h"

#include "ash/public/cpp/shelf_item_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"

ExtensionAppWindowLauncherItemController::
    ExtensionAppWindowLauncherItemController(const ash::ShelfID& shelf_id)
    : AppWindowLauncherItemController(shelf_id) {}

ExtensionAppWindowLauncherItemController::
    ~ExtensionAppWindowLauncherItemController() {}

void ExtensionAppWindowLauncherItemController::AddAppWindow(
    extensions::AppWindow* app_window) {
  AddWindow(app_window->GetBaseWindow());
}

ash::ShelfItemDelegate::AppMenuItems
ExtensionAppWindowLauncherItemController::GetAppMenuItems(int event_flags) {
  AppMenuItems items;
  extensions::AppWindowRegistry* app_window_registry =
      extensions::AppWindowRegistry::Get(
          ChromeLauncherController::instance()->profile());

  for (const ui::BaseWindow* window : windows()) {
    extensions::AppWindow* app_window =
        app_window_registry->GetAppWindowForNativeWindow(
            window->GetNativeWindow());
    DCHECK(app_window);

    // Use the app's web contents favicon, or the app window's icon.
    favicon::FaviconDriver* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(
            app_window->web_contents());
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

    items.push_back({app_window->GetTitle(), image});
  }
  return items;
}

void ExtensionAppWindowLauncherItemController::ExecuteCommand(
    bool from_context_menu,
    int64_t command_id,
    int32_t event_flags,
    int64_t display_id) {
  if (from_context_menu && ExecuteContextMenuCommand(command_id, event_flags))
    return;

  ChromeLauncherController::instance()->ActivateShellApp(app_id(), command_id);
}

void ExtensionAppWindowLauncherItemController::OnWindowTitleChanged(
    aura::Window* window) {
  ui::BaseWindow* base_window = GetAppWindow(window);
  extensions::AppWindowRegistry* app_window_registry =
      extensions::AppWindowRegistry::Get(
          ChromeLauncherController::instance()->profile());
  extensions::AppWindow* app_window =
      app_window_registry->GetAppWindowForNativeWindow(
          base_window->GetNativeWindow());

  // Use the window title (if set) to differentiate show_in_shelf window shelf
  // items instead of the default behavior of using the app name.
  if (app_window->show_in_shelf()) {
    base::string16 title = window->GetTitle();
    if (!title.empty())
      ChromeLauncherController::instance()->SetItemTitle(shelf_id(), title);
  }
}
