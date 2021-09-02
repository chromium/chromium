// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SIDE_PANEL_HELPER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SIDE_PANEL_HELPER_H_

namespace content {
struct OpenURLParams;
}  // namespace content

class Browser;

namespace lens {

// Opens the Lens side panel with the given Lens URL params.
void OpenLensSidePanel(Browser* browser,
                       const content::OpenURLParams& url_params);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SIDE_PANEL_HELPER_H_
