// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_VIEWS_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"

class CustomizeChromeUI;
class SidePanelUI;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace customize_chrome {

// Responsible for implementing logic to create and register/deregister the
// customize chrome side panel.
class SidePanelControllerViews : public SidePanelController {
 public:
  explicit SidePanelControllerViews(tabs::TabInterface& tab);
  SidePanelControllerViews(const SidePanelControllerViews&) = delete;
  SidePanelControllerViews& operator=(const SidePanelControllerViews&) = delete;
  ~SidePanelControllerViews() override;

  // SidePanelController:
  bool IsCustomizeChromeEntryAvailable() const override;
  bool IsCustomizeChromeEntryShowing() const override;
  void SetEntryChangedCallback(StateChangedCallBack callback) override;
  void SetCustomizeChromeSidePanelVisible(
      bool visible,
      CustomizeChromeSection section) override;
  void CreateAndRegisterEntry() override;
  void DeregisterEntry() override;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

 private:
  // Generates the view for the SidePanel contents. This is the WebUI for the
  // SidePanel. Used by the SidepanelRegistry to create the view.
  std::unique_ptr<views::View> CreateCustomizeChromeWebView();

  // Helper method for getting the SidePanelUI stored in the
  // BrowserWindowFeatures for the tab.
  SidePanelUI* GetSidePanelUI() const;

  // The Tab that is connected to this SidePanelControllerViews. It's safe to
  // assume that this tab_ will always be available because the tab interface
  // owns this object.
  const raw_ref<tabs::TabInterface> tab_;

  // Contents of the SidePanel for CustomizeChrome. This is only set if the
  // construction of the customize chrome page is done by
  // SidePanelControllerViews::CustomCreateCustomizeChromeWebView
  base::WeakPtr<CustomizeChromeUI> customize_chrome_ui_;

  // Caches a request to scroll to a section in case the request happens before
  // the front-end is ready to receive the request.
  std::optional<CustomizeChromeSection> section_;
  StateChangedCallBack entry_state_changed_callback_;
};

}  // namespace customize_chrome

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_VIEWS_H_
