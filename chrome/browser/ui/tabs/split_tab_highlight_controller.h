// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_HIGHLIGHT_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_HIGHLIGHT_CONTROLLER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class ElementIdentifier;
class TrackedElement;
}  // namespace ui

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace split_tabs {

// Coordinates when active tab in a split is highlighted.
class SplitTabHighlightController : public OmniboxTabHelper::Observer,
                                    public ChipController::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void SetHighlightActiveContentsView(bool is_highlighted) = 0;
  };

  explicit SplitTabHighlightController(BrowserWindowInterface* browser,
                                       Delegate* delegate);
  ~SplitTabHighlightController() override;

  bool ShouldHighlight();

  // OmniboxTabHelper::Observer:
  void OnOmniboxFocusChanged(OmniboxFocusState state,
                             OmniboxFocusChangeReason reason) override {}
  void OnOmniboxInputStateChanged() override {}
  void OnOmniboxInputInProgress(bool in_progress) override {}
  void OnOmniboxPopupVisibilityChanged(bool popup_is_open) override;

  // ChipController::Observer:
  void OnPermissionPromptShown() override;
  void OnPermissionPromptHidden() override;

 private:
  void AddShowHideElementSubscriptions(
      ui::ElementIdentifier element_identifier);
  void OnActiveTabChange(BrowserWindowInterface* browser_window_interface);
  void OnTabWillDetach(tabs::TabInterface* tab_interface,
                       tabs::TabInterface::DetachReason reason);
  void OnTabWillDiscard(tabs::TabInterface* tab_interface,
                        content::WebContents* old_contents,
                        content::WebContents* new_contents);
  void OnElementShown(ui::TrackedElement* tracked_element);
  void OnElementHidden(ui::TrackedElement* tracked_element);
  void UpdateHighlight();

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  raw_ptr<Delegate> split_tab_highlight_delegate_;

  bool is_permission_prompt_showing_ = false;
  bool is_omnibox_popup_showing_ = false;
  base::flat_map<ui::ElementIdentifier, bool> tracked_bubble_visibility_;
  std::vector<base::CallbackListSubscription> browser_scoped_subscriptions_;
  base::CallbackListSubscription tab_will_detach_subscription_;
  base::CallbackListSubscription tab_will_discard_subscription_;
  base::ScopedObservation<OmniboxTabHelper, OmniboxTabHelper::Observer>
      omnibox_tab_helper_observation_{this};
  base::ScopedObservation<ChipController, ChipController::Observer>
      chip_controller_observation_{this};
};

}  // namespace split_tabs

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_HIGHLIGHT_CONTROLLER_H_
