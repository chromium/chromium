// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_LINUX_X11_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_LINUX_X11_H_

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux.h"
#include "ui/views/widget/desktop_aura/x11_desktop_handler_observer.h"

// This is solely used by non-ozone X11 builds.
class ChromeBrowserMainExtraPartsViewsLinuxX11
    : public ChromeBrowserMainExtraPartsViewsLinux,
      public views::X11DesktopHandlerObserver {
 public:
  ChromeBrowserMainExtraPartsViewsLinuxX11();
  ~ChromeBrowserMainExtraPartsViewsLinuxX11() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PreCreateThreads() override;

  // Overridden from views::X11DesktopHandlerObserver.
  void OnWorkspaceChanged(const std::string& new_workspace) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsViewsLinuxX11);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_LINUX_X11_H_
