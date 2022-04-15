// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SIDE_PANEL_HELPER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SIDE_PANEL_HELPER_H_

#include "base/callback_forward.h"

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

// Opens the Lens region search bubble view with given params.
views::Widget* OpenLensRegionSearchInstructions(
    Browser* browser,
    base::OnceClosure close_callback,
    base::OnceClosure escape_callback);

// For testing purposes, retrieves the web contents used by the Lens side panel
// view.
content::WebContents* GetLensSidePanelWebContentsForTesting(Browser* browser);

// For testing purposes, creates the LensSidePanelController in the
// corresponding BrowserView of |browser|.
void CreateLensSidePanelControllerForTesting(Browser* browser);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SIDE_PANEL_HELPER_H_
