// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_UTILS_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_UTILS_H_

#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_tab_helper.h"

namespace customize_chrome {
std::unique_ptr<CustomizeChromeTabHelper::Delegate> CreateDelegate(
    content::WebContents* web_contents);
}  // namespace customize_chrome

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_UTILS_H_
