// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_LINUX_H_

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"
#include "ui/display/display_observer.h"

namespace ui {
class LinuxUiGetter;
#if defined(USE_DBUS)
class DarkModeManagerLinux;
#endif
}

// Extra parts, which are used by both Ozone/X11/Wayland and inherited by the
// non-ozone X11 extra parts.
class ChromeBrowserMainExtraPartsViewsLinux
    : public ChromeBrowserMainExtraPartsViews,
      public display::DisplayObserver {
 public:
  ChromeBrowserMainExtraPartsViewsLinux();

  ChromeBrowserMainExtraPartsViewsLinux(
      const ChromeBrowserMainExtraPartsViewsLinux&) = delete;
  ChromeBrowserMainExtraPartsViewsLinux& operator=(
      const ChromeBrowserMainExtraPartsViewsLinux&) = delete;

  ~ChromeBrowserMainExtraPartsViewsLinux() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void ToolkitInitialized() override;
  void PreCreateThreads() override;

 private:
  // display::DisplayObserver:
  void OnCurrentWorkspaceChanged(const std::string& new_workspace) override;

  std::optional<display::ScopedDisplayObserver> display_observer_;

  std::unique_ptr<ui::LinuxUiGetter> linux_ui_getter_;
#if defined(USE_DBUS)
  std::unique_ptr<ui::DarkModeManagerLinux> dark_mode_manager_;
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_LINUX_H_
