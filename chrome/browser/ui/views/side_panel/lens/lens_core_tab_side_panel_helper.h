// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_LENS_CORE_TAB_SIDE_PANEL_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_LENS_CORE_TAB_SIDE_PANEL_HELPER_H_

#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

namespace lens {

namespace internal {
// Returns if the v2 unified side panel is enabled.
// This checks all the basic requirements for the side panel to be enabled.
bool IsSidePanelEnabled(content::WebContents* web_contents);

// Helper to return the template url service from the given web contents.
TemplateURLService* GetTemplateURLService(content::WebContents* web_contents);

}  // namespace internal

// Returns the upper bound of the initial content area size of the side panel
// if the Lens side panel were to be opened or used right now.
gfx::Size GetSidePanelInitialContentSizeUpperBound(
    content::WebContents* web_contents);

// Returns if the v2 unified side panel is enabled when Google is the default
// search engine.
bool IsSidePanelEnabledForLens(content::WebContents* web_contents);

// Returns if the v2 unified side panel is enabled when Google is NOT the
// default search engine. The third party search engines needs to opt-in to the
// side panel experience so this checks those flags.
bool IsSidePanelEnabledFor3PDse(content::WebContents* web_contents);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_LENS_LENS_CORE_TAB_SIDE_PANEL_HELPER_H_
