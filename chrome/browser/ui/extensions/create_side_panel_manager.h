// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_CREATE_SIDE_PANEL_MANAGER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_CREATE_SIDE_PANEL_MANAGER_H_

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

// Implemented by extension_side_panel_manager.cc in views/.
void CreateSidePanelManagerForWebContents(Profile* profile,
                                          content::WebContents* web_contents);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_CREATE_SIDE_PANEL_MANAGER_H_
