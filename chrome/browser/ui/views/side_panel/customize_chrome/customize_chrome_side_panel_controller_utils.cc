// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_side_panel_controller_utils.h"

#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_side_panel_controller.h"

namespace customize_chrome {

std::unique_ptr<CustomizeChromeTabHelper::Delegate> CreateDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<CustomizeChromeSidePanelController>(web_contents);
}

}  // namespace customize_chrome
