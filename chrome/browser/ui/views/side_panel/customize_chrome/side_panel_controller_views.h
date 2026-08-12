// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_VIEWS_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller_base.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_native_view.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"

class CustomizeChromeUI;

namespace customize_chrome {

// Desktop implementation of the controller.
class SidePanelControllerViews : public SidePanelControllerBase {
 public:
  explicit SidePanelControllerViews(tabs::TabInterface& tab);
  SidePanelControllerViews(const SidePanelControllerViews&) = delete;
  SidePanelControllerViews& operator=(const SidePanelControllerViews&) = delete;
  ~SidePanelControllerViews() override;

  // SidePanelControllerBase:
  void OpenSidePanel(SidePanelOpenTrigger trigger,
                     std::optional<CustomizeChromeSection> section) override;

 private:
  // SidePanelControllerBase:
  void OnEntryRegisteredForUrl(const GURL& url) override;

  // Returns true for 1P NTP or extension NTP, otherwise returns false.
  bool ShouldEnableEditTheme(const GURL& url) const;

  // Generates the view for the SidePanel contents. This is the WebUI for the
  // SidePanel. Used by the SidepanelRegistry to create the view.
  SidePanelNativeView CreateCustomizeChromeView(
      SidePanelEntryScope& scope) override;

  // Contents of the SidePanel for CustomizeChrome. This is only set if the
  // construction of the customize chrome page is done by
  // SidePanelController::CustomCreateCustomizeChromeWebView
  base::WeakPtr<CustomizeChromeUI> customize_chrome_ui_;

  // Caches a request to scroll to a section in case the request happens before
  // the front-end is ready to receive the request.
  std::optional<CustomizeChromeSection> section_;
};

}  // namespace customize_chrome

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_VIEWS_H_
