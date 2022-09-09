// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"

namespace content {
class WebContents;
}  // namespace content

// Responsible for implementing logic to create and register/deregister
// the side panel.
class CustomizeChromeSidePanelController
    : public CustomizeChromeTabHelper::Delegate,
      public SidePanelEntryObserver {
 public:
  explicit CustomizeChromeSidePanelController(
      content::WebContents* web_contents);
  CustomizeChromeSidePanelController(
      const CustomizeChromeSidePanelController&) = delete;
  CustomizeChromeSidePanelController& operator=(
      const CustomizeChromeSidePanelController&) = delete;
  ~CustomizeChromeSidePanelController() override;

  // CustomizeChromeTabHelper::Delegate
  void CreateAndRegisterEntry() override;
  void DeregisterEntry() override;
  void ShowCustomizeChromeSidePanel() override;
  bool IsCustomizeChromeEntryShowing() const override;
  bool IsCustomizeChromeEntryAvailable() const override;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

 private:
  // Creates view for side panel entry.
  std::unique_ptr<views::View> CreateCustomizeChromeWebView();

  BrowserView* GetBrowserView() const;

  const raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_H_
