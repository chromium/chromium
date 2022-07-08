// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_side_panel_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/lens/lens_region_search_instructions_view.h"
#include "chrome/browser/ui/views/lens/lens_side_panel_controller.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/views/widget/widget.h"

namespace lens {

void OpenLensSidePanel(Browser* browser,
                       const content::OpenURLParams& url_params) {
  if (base::FeatureList::IsEnabled(features::kUnifiedSidePanel)) {
    LensSidePanelCoordinator::GetOrCreateForBrowser(browser)
        ->RegisterEntryAndShow(url_params);
  } else {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

    if (!browser_view->lens_side_panel_controller())
      browser_view->CreateLensSidePanelController();

    browser_view->lens_side_panel_controller()->OpenWithURL(url_params);
  }
}

views::Widget* OpenLensRegionSearchInstructions(
    Browser* browser,
    base::OnceClosure close_callback,
    base::OnceClosure escape_callback) {
  // Our anchor should be the browser view's ContentsWebView to make the bubble
  // dialog align with the viewport.
  views::View* anchor =
      BrowserView::GetBrowserViewForBrowser(browser)->contents_web_view();
  return views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<LensRegionSearchInstructionsView>(
          anchor, std::move(close_callback), std::move(escape_callback)));
}

void CreateLensSidePanelControllerForTesting(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  browser_view->CreateLensSidePanelController();
  DCHECK(browser_view->lens_side_panel_controller());
}

content::WebContents* GetLensSidePanelWebContentsForTesting(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  DCHECK(browser_view->lens_side_panel_controller());
  return browser_view->lens_side_panel_controller()->web_contents();
}

void CreateLensUnifiedSidePanelEntryForTesting(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  DCHECK(browser_view->side_panel_coordinator());
  browser_view->side_panel_coordinator()->SetNoDelaysForTesting();

  auto* lens_side_panel_coordinator =
      LensSidePanelCoordinator::GetOrCreateForBrowser(browser);
  DCHECK(lens_side_panel_coordinator);
  lens_side_panel_coordinator->RegisterEntryAndShow(
      content::OpenURLParams(GURL("http://foo.com"), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
  DCHECK(lens_side_panel_coordinator->GetViewWebContentsForTesting());
}

content::WebContents* GetLensUnifiedSidePanelWebContentsForTesting(
    Browser* browser) {
  auto* lens_side_panel_coordinator =
      LensSidePanelCoordinator::FromBrowser(browser);
  DCHECK(lens_side_panel_coordinator);
  auto* web_contents =
      lens_side_panel_coordinator->GetViewWebContentsForTesting();
  DCHECK(web_contents);
  return web_contents;
}

}  // namespace lens
