// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_

#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"

namespace content {
class WebContents;
}  // namespace content

// Responsible for implementing logic to create and register/deregister
// the side panel.
class CustomizeChromeSidePanelController
    : public CustomizeChromeTabHelper::Delegate {
 public:
  CustomizeChromeSidePanelController();
  CustomizeChromeSidePanelController(
      const CustomizeChromeSidePanelController&) = delete;
  CustomizeChromeSidePanelController& operator=(
      const CustomizeChromeSidePanelController&) = delete;
  ~CustomizeChromeSidePanelController() override;

  // CustomizeChromeTabHelper::Delegate
  void CreateAndRegisterEntry(content::WebContents* web_contents) override;
  void DeregisterEntry(content::WebContents* web_contents) override;

 private:
  // Creates view for side panel entry.
  std::unique_ptr<views::View> CreateCustomizeChromeWebView(
      content::WebContents* web_contents);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_
