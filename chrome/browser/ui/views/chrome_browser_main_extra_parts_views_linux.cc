// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux.h"

#include "chrome/browser/themes/theme_service_aura_linux.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/theme_profile_key.h"
#include "ui/display/screen.h"
#include "ui/views/linux_ui/linux_ui.h"

#if BUILDFLAG(USE_GTK)
#include "ui/gtk/gtk_ui.h"
#include "ui/gtk/gtk_ui_delegate.h"
#endif

#if defined(USE_OZONE)
#include "ui/base/cursor/cursor_factory.h"
#endif

#if defined(USE_X11)
#include "ui/gfx/x/connection.h"  // nogncheck
#if BUILDFLAG(USE_GTK)
#include "ui/base/ui_base_features.h"
#include "ui/gtk/x/gtk_ui_delegate_x11.h"  // nogncheck
#endif  // BUILDFLAG(USE_GTK)
#endif  // defined(USE_X11)

namespace {

views::LinuxUI* BuildLinuxUI() {
  views::LinuxUI* linux_ui = nullptr;
  // GtkUi is the only LinuxUI implementation for now.
#if BUILDFLAG(USE_GTK)
  if (ui::GtkUiDelegate::instance())
    linux_ui = BuildGtkUi(ui::GtkUiDelegate::instance());
#endif
  return linux_ui;
}

}  // namespace

ChromeBrowserMainExtraPartsViewsLinux::ChromeBrowserMainExtraPartsViewsLinux() =
    default;

ChromeBrowserMainExtraPartsViewsLinux::
    ~ChromeBrowserMainExtraPartsViewsLinux() {
  // It's not expected that the screen is destroyed by this point, but it can happen during fuzz
  // tests.
  if (display::Screen::GetScreen())
    display::Screen::GetScreen()->RemoveObserver(this);
}

void ChromeBrowserMainExtraPartsViewsLinux::ToolkitInitialized() {
#if defined(USE_X11) && BUILDFLAG(USE_GTK)
  if (!features::IsUsingOzonePlatform()) {
    // In Aura/X11, Gtk-based LinuxUI implementation is used, so we instantiate
    // and inject the GtkUiDelegate before
    // ChromeBrowserMainExtraPartsViewsLinux, so it can properly initialize
    // GtkUi on its |ToolkitInitialized| override.
    gtk_ui_delegate_ =
        std::make_unique<ui::GtkUiDelegateX11>(x11::Connection::Get());
    ui::GtkUiDelegate::SetInstance(gtk_ui_delegate_.get());
  }
#endif

  ChromeBrowserMainExtraPartsViews::ToolkitInitialized();

  views::LinuxUI* linux_ui = BuildLinuxUI();
  if (!linux_ui)
    return;

  linux_ui->SetUseSystemThemeCallback(
      base::BindRepeating([](aura::Window* window) {
        if (!window)
          return true;
        return ThemeServiceAuraLinux::ShouldUseSystemThemeForProfile(
            GetThemeProfileForWindow(window));
      }));

  // Update the device scale factor before initializing views
  // because its display::Screen instance depends on it.
  linux_ui->UpdateDeviceScaleFactor();

  views::LinuxUI::SetInstance(linux_ui);
  linux_ui->Initialize();

  // Cursor theme changes are tracked by LinuxUI (via a CursorThemeManager
  // implementation). Start observing them once it's initialized.
  ui::CursorFactory::GetInstance()->ObserveThemeChanges();

  DCHECK(ui::LinuxInputMethodContextFactory::instance())
      << "LinuxUI must set LinuxInputMethodContextFactory instance.";
}

void ChromeBrowserMainExtraPartsViewsLinux::PreCreateThreads() {
  ChromeBrowserMainExtraPartsViews::PreCreateThreads();
  // We could do that during the ToolkitInitialized call, which is called before
  // this method, but the display::Screen is only created after PreCreateThreads
  // is called. Thus, do that here instead.
  display::Screen::GetScreen()->AddObserver(this);
}

void ChromeBrowserMainExtraPartsViewsLinux::OnCurrentWorkspaceChanged(
    const std::string& new_workspace) {
  BrowserList::MoveBrowsersInWorkspaceToFront(new_workspace);
}
