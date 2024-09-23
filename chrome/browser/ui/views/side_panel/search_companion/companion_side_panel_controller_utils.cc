// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/companion/companion_side_panel_controller_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/search_companion/companion_side_panel_controller.h"

namespace companion {

std::unique_ptr<CompanionTabHelper::Delegate> CreateDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<CompanionSidePanelController>(web_contents);
}

Browser* GetBrowserForWebContents(content::WebContents* web_contents) {
  auto* browser_window =
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents);
  auto* browser_view = static_cast<BrowserView*>(browser_window);
  return browser_view ? browser_view->browser() : nullptr;
}

}  // namespace companion
