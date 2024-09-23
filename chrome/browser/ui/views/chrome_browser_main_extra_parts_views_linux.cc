// Copyright 2016 The Chromium Authors
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
#include "ui/linux/linux_ui_getter.h"
#include "ui/native_theme/native_theme.h"
#include "ui/ozone/public/ozone_platform.h"

#if defined(USE_DBUS)
#include "chrome/browser/ui/views/dark_mode_manager_linux.h"
#endif

namespace {

class LinuxUiGetterImpl : public ui::LinuxUiGetter {
 public:
  LinuxUiGetterImpl() = default;
  ~LinuxUiGetterImpl() override = default;
  ui::LinuxUiTheme* GetForWindow(aura::Window* window) override {
    return window ? GetForProfile(GetThemeProfileForWindow(window)) : nullptr;
  }
  ui::LinuxUiTheme* GetForProfile(Profile* profile) override {
    return ui::GetLinuxUiTheme(
        ThemeServiceAuraLinux::GetSystemThemeForProfile(profile));
  }
};

}  // namespace

ChromeBrowserMainExtraPartsViewsLinux::ChromeBrowserMainExtraPartsViewsLinux() =
    default;

ChromeBrowserMainExtraPartsViewsLinux::
    ~ChromeBrowserMainExtraPartsViewsLinux() = default;

void ChromeBrowserMainExtraPartsViewsLinux::ToolkitInitialized() {
  ChromeBrowserMainExtraPartsViews::ToolkitInitialized();

  if (auto* linux_ui = ui::GetDefaultLinuxUi()) {
    linux_ui_getter_ = std::make_unique<LinuxUiGetterImpl>();
    ui::LinuxUi::SetInstance(linux_ui);

    // Cursor theme changes are tracked by LinuxUI (via a CursorThemeManager
    // implementation). Start observing them once it's initialized.
    ui::CursorFactory::GetInstance()->ObserveThemeChanges();
  }
#if defined(USE_DBUS)
  dark_mode_manager_ = std::make_unique<ui::DarkModeManagerLinux>();
#endif
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
