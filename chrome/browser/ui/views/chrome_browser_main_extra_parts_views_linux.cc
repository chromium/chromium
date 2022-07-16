// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux.h"

#include "chrome/browser/themes/theme_service_aura_linux.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/theme_profile_key.h"
#include "ui/base/buildflags.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/ime/linux/fake_input_method_context_factory.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/linux/linux_ui_delegate.h"
#include "ui/ozone/public/ozone_platform.h"

#if BUILDFLAG(USE_GTK)
#include "ui/gtk/gtk_ui_factory.h"
#endif

namespace {

std::unique_ptr<views::LinuxUI> BuildLinuxUI() {
  // If the ozone backend hasn't provided a LinuxUiDelegate, don't try to create
  // a LinuxUi instance as this may result in a crash in toolkit initialization.
  if (!ui::LinuxUiDelegate::GetInstance())
    return nullptr;

  // GtkUi is the only LinuxUI implementation for now.
#if BUILDFLAG(USE_GTK)
  return BuildGtkUi();
#else
  return nullptr;
#endif
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

    linux_ui->Initialize();
    views::LinuxUI::SetInstance(std::move(linux_ui));

    // Cursor theme changes are tracked by LinuxUI (via a CursorThemeManager
    // implementation). Start observing them once it's initialized.
    ui::CursorFactory::GetInstance()->ObserveThemeChanges();
  } else {
    // In case if GTK is not used, input method factory won't be set for X11 and
    // Ozone/X11. Set a fake one instead to avoid crashing browser later.
    DCHECK(!ui::LinuxInputMethodContextFactory::instance());
    // Try to create input method through Ozone so that the backend has a chance
    // to set factory by itself.
    ui::OzonePlatform::GetInstance()->CreateInputMethod(
        nullptr, gfx::kNullAcceleratedWidget);
  }
  // If factory is not set, set a fake instance.
  if (!ui::LinuxInputMethodContextFactory::instance()) {
    ui::LinuxInputMethodContextFactory::SetInstance(
        new ui::FakeInputMethodContextFactory());
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
