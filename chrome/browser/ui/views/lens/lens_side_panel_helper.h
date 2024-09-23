// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_HELPER_H_

#include "base/functional/callback_forward.h"
#include "url/gurl.h"

namespace content {
class WebContents;
struct OpenURLParams;
}  // namespace content

namespace views {
class Widget;
}

class Browser;

namespace lens {

// Opens the Lens side panel with the given Lens URL params.
void OpenLensSidePanel(Browser* browser,
                       const content::OpenURLParams& url_params);

// Opens the Lens region search feature in a new tab with a WebUI page.
void OpenLensStaticPage(Browser* browser);

// Checks to see if the page corresponding to the current URL should be visible
// to the user. The page should be visible if
//   - The page is the Lens Results page
//   - The page does not correspond to the Lens domain
//   - lens::features::kEnableLensHtmlRedirectFix is set to false
bool ShouldPageBeVisible(const GURL& url);

// Creates URL for opening a lens result in a new tab
// with appropriate entry point parameters
GURL CreateURLForNewTab(const GURL& original_url);

// Opens the Lens region search bubble view with given params.
views::Widget* OpenLensRegionSearchInstructions(
    Browser* browser,
    base::OnceClosure close_callback,
    base::OnceClosure escape_callback);

// For testing purposes, retrieves the web contents used by the lens unified
// side panel view. This is there because of BUILD rules for browser test where
// you can't include files from views folder as deps.
content::WebContents* GetLensUnifiedSidePanelWebContentsForTesting(
    Browser* browser);

// For testing purposes, creates the lens side panel coordinator and ensures
// that it has an empty view
void CreateLensUnifiedSidePanelEntryForTesting(Browser* browser);
}  // namespace lens

#endif  // CHROME_BROWSER_UI_VIEWS_LENS_LENS_SIDE_PANEL_HELPER_H_
