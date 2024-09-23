// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_VIEWS_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "content/public/browser/web_contents_observer.h"

class CustomizeChromeUI;
class SidePanelUI;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace views {
class View;
}

namespace customize_chrome {

// Responsible for implementing logic to create and register/deregister the
// customize chrome side panel. This implementation listens to the webcontents
// for a given tab and, on navigation completion, registers the sidepanel entry
// for the tab, if its possible to show the CustomizeChrome sidepanel.
class SidePanelControllerViews : public SidePanelController,
                                 public content::WebContentsObserver {
 public:
  explicit SidePanelControllerViews(tabs::TabInterface& tab);
  SidePanelControllerViews(const SidePanelControllerViews&) = delete;
  SidePanelControllerViews& operator=(const SidePanelControllerViews&) = delete;
  ~SidePanelControllerViews() override;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  // SidePanelController:
  bool IsCustomizeChromeEntryAvailable() const override;
  bool IsCustomizeChromeEntryShowing() const override;
  void SetEntryChangedCallback(StateChangedCallBack callback) override;
  void CreateAndRegisterEntry() override;
  void DeregisterEntry() override;
  void OpenSidePanel(SidePanelOpenTrigger trigger,
                     std::optional<CustomizeChromeSection> section) override;
  void CloseSidePanel() override;

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Returns whether the SidePanel should be allowed to show on a given URL.
  // Currently this limits to the New Tab Page only.
  bool CanShowOnURL(const GURL& url) const;

  // Generates the view for the SidePanel contents. This is the WebUI for the
  // SidePanel. Used by the SidepanelRegistry to create the view.
  std::unique_ptr<views::View> CreateCustomizeChromeWebView();

  // Helper method for getting the SidePanelUI stored in the
  // BrowserWindowFeatures for the tab.
  SidePanelUI* GetSidePanelUI() const;

  // Implementation of TabInterfaces RegisterWillDiscardContents callback.
  // Updates the webcontentsobserver so that the SidePanelController can listen
  // to navigations of the tab.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* previous_contents,
                           content::WebContents* new_contents);

  // The Tab that is connected to this SidePanelController. It's safe to assume
  // that this tab_ will always be available because the tab interface owns this
  // object.
  const raw_ref<tabs::TabInterface> tab_;

  // Contents of the SidePanel for CustomizeChrome. This is only set if the
  // construction of the customize chrome page is done by
  // SidePanelController::CustomCreateCustomizeChromeWebView
  base::WeakPtr<CustomizeChromeUI> customize_chrome_ui_;

  // Caches a request to scroll to a section in case the request happens before
  // the front-end is ready to receive the request.
  std::optional<CustomizeChromeSection> section_;
  StateChangedCallBack entry_state_changed_callback_;

  // Subscription for the discard of the tab.
  base::CallbackListSubscription will_discard_contents_callback_subscription_;
};

}  // namespace customize_chrome

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_VIEWS_H_
