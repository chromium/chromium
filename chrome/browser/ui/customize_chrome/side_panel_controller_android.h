// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_ANDROID_H_

#include "chrome/browser/ui/customize_chrome/side_panel_controller_base.h"
#include "chrome/browser/ui/side_panel/side_panel_native_view.h"

namespace customize_chrome {

class SidePanelControllerAndroid : public SidePanelControllerBase {
 public:
  explicit SidePanelControllerAndroid(tabs::TabInterface& tab);
  SidePanelControllerAndroid(const SidePanelControllerAndroid&) = delete;
  SidePanelControllerAndroid& operator=(const SidePanelControllerAndroid&) =
      delete;
  ~SidePanelControllerAndroid() override;

 private:
  SidePanelNativeView CreateCustomizeChromeView(
      SidePanelEntryScope& scope) override;
};

}  // namespace customize_chrome

#endif  // CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_ANDROID_H_
