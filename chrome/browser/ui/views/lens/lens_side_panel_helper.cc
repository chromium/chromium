// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_side_panel_helper.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/lens/lens_region_search_instructions_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_rendering_environment.h"
#include "components/lens/lens_url_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/url_util.h"
#include "ui/views/widget/widget.h"

namespace lens {

views::Widget* OpenLensRegionSearchInstructions(
    Browser* browser,
    base::OnceClosure close_callback,
    base::OnceClosure escape_callback) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  CHECK(browser_view);
  // Our anchor should be the browser view's top container view. This makes sure
  // that we account for side panel width and the top container view.
  views::View* anchor = browser_view->contents_web_view();
  return views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<LensRegionSearchInstructionsView>(
          anchor, std::move(close_callback), std::move(escape_callback)));
}

}  // namespace lens
