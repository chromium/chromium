// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_side_panel_helper.h"

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/lens/lens_region_search_instructions_view.h"
#include "chrome/browser/ui/views/lens/lens_static_page_controller.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_rendering_environment.h"
#include "components/lens/lens_url_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/url_util.h"
#include "ui/views/widget/widget.h"

namespace lens {

bool IsValidLensResultUrl(const GURL& url) {
  if (url.is_empty())
    return false;

  std::string payload;
  // Make sure the payload is present
  return net::GetValueForKeyInQuery(url, kPayloadQueryParameter, &payload);
}

bool IsLensUrl(const GURL& url) {
  return !url.is_empty() &&
         url.host() == GURL(lens::features::GetHomepageURLForLens()).host();
}

bool ShouldPageBeVisible(const GURL& url) {
  return lens::IsValidLensResultUrl(url) || !lens::IsLensUrl(url) ||
         !lens::features::GetEnableLensHtmlRedirectFix();
}

// We need to create a new URL with the specified query parameters while
// also keeping the payload parameter in the original URL.
GURL CreateURLForNewTab(const GURL& original_url) {
  if (!IsValidLensResultUrl(original_url))
    return GURL();
  // Set the side panel max size to zero, as this is not a side panel request.
  gfx::Size side_panel_initial_size = gfx::Size();

  // Append or replace query parameters related to entry point.
  return AppendOrReplaceQueryParametersForLensRequest(
      original_url, EntryPoint::CHROME_OPEN_NEW_TAB_SIDE_PANEL,
      RenderingEnvironment::ONELENS_DESKTOP_WEB_FULLSCREEN,
      /*is_side_panel_request=*/false, side_panel_initial_size);
}

void OpenLensSidePanel(Browser* browser,
                       const content::OpenURLParams& url_params) {
  LensSidePanelCoordinator::GetOrCreateForBrowser(browser)
      ->RegisterEntryAndShow(url_params);
}

views::Widget* OpenLensRegionSearchInstructions(
    Browser* browser,
    base::OnceClosure close_callback,
    base::OnceClosure escape_callback) {
  // Our anchor should be the browser view's top container view. This makes sure
  // that we account for side panel width and the top container view.
  views::View* anchor =
      BrowserView::GetBrowserViewForBrowser(browser)->contents_web_view();
  return views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<LensRegionSearchInstructionsView>(
          anchor, std::move(close_callback), std::move(escape_callback)));
}

void CreateLensUnifiedSidePanelEntryForTesting(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  SidePanelCoordinator* coordinator = browser_view->side_panel_coordinator();
  DCHECK(coordinator);
  coordinator->SetNoDelaysForTesting(true);  // IN-TEST

  auto* lens_side_panel_coordinator =
      LensSidePanelCoordinator::GetOrCreateForBrowser(browser);
  DCHECK(lens_side_panel_coordinator);
  lens_side_panel_coordinator->RegisterEntryAndShow(content::OpenURLParams(
      GURL(lens::features::GetHomepageURLForLens()), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false));
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

void OpenLensStaticPage(Browser* browser) {
  DCHECK(browser);
  auto lens_static_page_data = std::make_unique<lens::LensStaticPageData>();
  lens_static_page_data->lens_static_page_controller =
      std::make_unique<lens::LensStaticPageController>(browser);
  lens_static_page_data->lens_static_page_controller->OpenStaticPage();
  browser->SetUserData(LensStaticPageData::kDataKey,
                       std::move(lens_static_page_data));
}

}  // namespace lens
