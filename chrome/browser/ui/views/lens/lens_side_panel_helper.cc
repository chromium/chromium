// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_side_panel_helper.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/lens/lens_side_panel_controller.h"

namespace lens {

void OpenLensSidePanel(Browser* browser,
                       const content::OpenURLParams& url_params) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  browser_view->lens_side_panel_controller()->OpenWithURL(url_params);
}

}  // namespace lens
