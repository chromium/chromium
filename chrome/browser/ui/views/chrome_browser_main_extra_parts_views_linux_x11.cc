// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux_x11.h"

#include "chrome/browser/ui/browser_list.h"
#include "ui/views/widget/desktop_aura/x11_desktop_handler.h"

ChromeBrowserMainExtraPartsViewsLinuxX11::
    ChromeBrowserMainExtraPartsViewsLinuxX11() = default;

ChromeBrowserMainExtraPartsViewsLinuxX11::
    ~ChromeBrowserMainExtraPartsViewsLinuxX11() {
  if (views::X11DesktopHandler::get_dont_create())
    views::X11DesktopHandler::get_dont_create()->RemoveObserver(this);
}

void ChromeBrowserMainExtraPartsViewsLinuxX11::PreCreateThreads() {
  ChromeBrowserMainExtraPartsViewsLinux::PreCreateThreads();
  views::X11DesktopHandler::get()->AddObserver(this);
}

void ChromeBrowserMainExtraPartsViewsLinuxX11::OnWorkspaceChanged(
    const std::string& new_workspace) {
  BrowserList::MoveBrowsersInWorkspaceToFront(new_workspace);
}
