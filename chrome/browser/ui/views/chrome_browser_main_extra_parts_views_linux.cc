// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux.h"

#include "chrome/browser/themes/theme_service_aura_linux.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/theme_profile_key.h"
#include "ui/base/buildflags.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_delegate.h"
#include "ui/linux/linux_ui_factory.h"
#include "ui/ozone/public/ozone_platform.h"

namespace {

std::unique_ptr<ui::LinuxUi> BuildLinuxUI() {
  // If the ozone backend hasn't provided a LinuxUiDelegate, don't try to create
  // a LinuxUi instance as this may result in a crash in toolkit initialization.
  if (!ui::LinuxUiDelegate::GetInstance())
    return nullptr;

  return ui::CreateLinuxUi();
}

}  // namespace

ChromeBrowserMainExtraPartsViewsLinux::ChromeBrowserMainExtraPartsViewsLinux() =
    default;

ChromeBrowserMainExtraPartsViewsLinux::
    ~ChromeBrowserMainExtraPartsViewsLinux() = default;

void ChromeBrowserMainExtraPartsViewsLinux::ToolkitInitialized() {
  ChromeBrowserMainExtraPartsViews::ToolkitInitialized();

  if (auto linux_ui = BuildLinuxUI()) {
    linux_ui->SetUseSystemThemeCallback(
        base::BindRepeating([](aura::Window* window) {
          if (!window)
            return true;
          return ThemeServiceAuraLinux::ShouldUseSystemThemeForProfile(
              GetThemeProfileForWindow(window));
        }));
    ui::LinuxUi::SetInstance(std::move(linux_ui));

    // Cursor theme changes are tracked by LinuxUI (via a CursorThemeManager
    // implementation). Start observing them once it's initialized.
    ui::CursorFactory::GetInstance()->ObserveThemeChanges();
  }
}

void ChromeBrowserMainExtraPartsViewsLinux::PreCreateThreads() {
  ChromeBrowserMainExtraPartsViews::PreCreateThreads();
  // We could do that during the ToolkitInitialized call, which is called before
  // this method, but the display::Screen is only created after PreCreateThreads
  // is called. Thus, do that here instead.
  display_observer_.emplace(this);
}

void ChromeBrowserMainExtraPartsViewsLinux::OnCurrentWorkspaceChanged(
    const std::string& new_workspace) {
  BrowserList::MoveBrowsersInWorkspaceToFront(new_workspace);
}
