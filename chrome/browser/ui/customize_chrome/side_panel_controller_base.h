// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_BASE_H_
#define CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_BASE_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_native_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class GURL;
class SidePanelUI;
class SidePanelEntryScope;

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace customize_chrome {

// Responsible for implementing logic to create and register the
// customize chrome side panel. This implementation listens to the webcontents
// for a given tab and, on navigation completion, registers the sidepanel entry
// for the tab, if its possible to show the CustomizeChrome sidepanel.
// This base class contains all the code that is not platform-dependent.
class SidePanelControllerBase : public SidePanelController,
                                public content::WebContentsObserver {
 public:
  explicit SidePanelControllerBase(tabs::TabInterface& tab);
  SidePanelControllerBase(const SidePanelControllerBase&) = delete;
  SidePanelControllerBase& operator=(const SidePanelControllerBase&) = delete;
  ~SidePanelControllerBase() override;

  // SidePanelController:
  bool IsCustomizeChromeEntryAvailable() const override;
  bool IsCustomizeChromeEntryShowing() const override;
  void SetEntryChangedCallback(StateChangedCallBack callback) override;
  void OpenSidePanel(SidePanelOpenTrigger trigger,
                     std::optional<CustomizeChromeSection> section) override;
  void CloseSidePanel() override;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;
  void OnEntryWillHide(SidePanelEntry* entry,
                       SidePanelEntryHideReason reason) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 protected:
  // Helper method for getting the SidePanelUI stored in the
  // BrowserWindowFeatures for the tab.
  SidePanelUI* GetSidePanelUI() const;

  // Generates the view for the SidePanel contents. This is the WebUI for the
  // SidePanel. Used by the SidepanelRegistry to create the view.
  virtual SidePanelNativeView CreateCustomizeChromeView(
      SidePanelEntryScope& scope) = 0;
  virtual void OnEntryRegisteredForUrl(const GURL& url) {}

  // The Tab that is connected to this SidePanelController. It's safe to assume
  // that this tab_ will always be available because the tab interface owns this
  // object.
  const raw_ref<tabs::TabInterface> tab_;

 private:
  // Registers the entry if the tab has a registry and no existing entry.
  void CreateAndRegisterEntry();

  // Returns whether the SidePanel should be allowed to show on a given URL.
  bool CanShowOnURL(const GURL& url) const;
  // tabs::TabInterface callback:
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* previous_contents,
                           content::WebContents* new_contents);

  // Subscription for the discard of the tab.
  base::CallbackListSubscription will_discard_contents_callback_subscription_;
  StateChangedCallBack entry_state_changed_callback_;
  ui::ScopedUnownedUserData<SidePanelController> scoped_unowned_user_data_;
};

}  // namespace customize_chrome

#endif  // CHROME_BROWSER_UI_CUSTOMIZE_CHROME_SIDE_PANEL_CONTROLLER_BASE_H_
