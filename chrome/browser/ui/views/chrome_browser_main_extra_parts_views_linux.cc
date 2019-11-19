// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux.h"

#include "chrome/browser/themes/theme_service_aura_linux.h"
#include "chrome/browser/ui/views/linux_ui/linux_ui_factory.h"
#include "chrome/browser/ui/views/theme_profile_key.h"
#include "ui/views/linux_ui/linux_ui.h"

ChromeBrowserMainExtraPartsViewsLinux::ChromeBrowserMainExtraPartsViewsLinux() =
    default;

ChromeBrowserMainExtraPartsViewsLinux::
    ~ChromeBrowserMainExtraPartsViewsLinux() = default;

void ChromeBrowserMainExtraPartsViewsLinux::PreEarlyInitialization() {
  views::LinuxUI* linux_ui = views::BuildLinuxUI();
  if (!linux_ui)
    return;

  linux_ui->SetUseSystemThemeCallback(
      base::BindRepeating([](aura::Window* window) {
        if (!window)
          return true;
        return ThemeServiceAuraLinux::ShouldUseSystemThemeForProfile(
            GetThemeProfileForWindow(window));
      }));
  views::LinuxUI::SetInstance(linux_ui);
}

void ChromeBrowserMainExtraPartsViewsLinux::ToolkitInitialized() {
  ChromeBrowserMainExtraPartsViews::ToolkitInitialized();
  auto* instance = views::LinuxUI::instance();
  if (instance)
    instance->Initialize();
}

void ChromeBrowserMainExtraPartsViewsLinux::PreCreateThreads() {
  // Update the device scale factor before initializing views
  // because its display::Screen instance depends on it.
  auto* instance = views::LinuxUI::instance();
  if (instance)
    instance->UpdateDeviceScaleFactor();
  ChromeBrowserMainExtraPartsViews::PreCreateThreads();
}
