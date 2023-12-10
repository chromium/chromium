// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_H_
#define CHROME_BROWSER_UI_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_H_

class GURL;

namespace content {
class WebContents;
}  // namespace content

// Implemented by about_this_site_side_panel_coordinator.cc in ui/views.
void ShowAboutThisSiteSidePanel(content::WebContents* web_contents,
                                const GURL& more_about_url);
void RegisterAboutThisSiteSidePanel(content::WebContents* web_contents,
                                    const GURL& more_about_url);
#endif  // CHROME_BROWSER_UI_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_H_
