// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_WEBUI_PAGE_ACTION_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_WEBUI_PAGE_ACTION_CONTROL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_model_observer.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "mojo/public/mojom/base/error.mojom-forward.h"

namespace content {
class WebContents;
}

namespace actions {
class ActionItem;
}

namespace tabs {
class TabInterface;
}

class BrowserWindowInterface;
class WebUIToolbarControlDelegate;

namespace page_actions {
class AnchoredMessageBubbleView;
class PageActionModelInterface;
class PageActionViewInterface;
class WebUIPageActionView;

// WebUIPageActionControl bridges the page actions backend (managed by
// PageActionController) to the WebUI toolbar. It manages a set of delegates
// (one per PageAction) that observe the active tab's page action models and
// notify the WebUI of state changes. It also routes user interactions (clicks,
// chip visibility changes) from the WebUI back to the backend by invoking
// the corresponding ActionItems.
//
// Owned by WebUILocationBar (which is owned by WebUIToolbarWebView).
class WebUIPageActionControl {
 public:
  explicit WebUIPageActionControl(actions::ActionItem* root_action_item);
  WebUIPageActionControl(const WebUIPageActionControl&) = delete;
  WebUIPageActionControl& operator=(const WebUIPageActionControl&) = delete;
  ~WebUIPageActionControl();

  void Init(WebUIToolbarControlDelegate* webui_delegate);

  // Updates the active controller based on the active tab of the browser.
  void UpdateController(content::WebContents* web_contents);

  // Asks the active controller to update visibility of page actions.
  void SetShouldHidePageActions(bool should_hide_page_actions);

  // Re-resolves and pushes the state of all page actions. Needed because
  // `PageActionState` carries resolved `SkColor`s (the color overrides and the
  // blended `kCascadingAccent` icon color), which otherwise go stale on a
  // same-tab theme change. Mirrors `PageActionView::OnThemeChanged()`.
  void OnThemeChanged();

  // Returns the current state of all visible page actions for WebUI. When
  // `commit_to_cache` is true, updates each delegate's cached `old_state_`.
  std::vector<toolbar_ui_api::mojom::PageActionStatePtr> GetPageActionStates(
      bool commit_to_cache = false);

  // Returns the PageActionViewInterface for the given action id.
  PageActionViewInterface* GetPageActionViewInterface(
      actions::ActionId action_id);

  // Handles a click on a page action from WebUI.
  void OnPageActionClick(
      toolbar_ui_api::mojom::PageActionId action_id,
      PageActionTrigger trigger,
      toolbar_ui_api::mojom::ToolbarUIService::OnPageActionClickCallback
          callback);

  // Handles chip showing changed notification from WebUI.
  void OnPageActionChipShowingChanged(
      toolbar_ui_api::mojom::PageActionId action_id,
      bool is_showing,
      toolbar_ui_api::mojom::ToolbarUIService::
          OnPageActionChipShowingChangedCallback callback);

  // Helpers accessed by WebUIPageActionView:
  BrowserWindowInterface* GetBrowser();
  const page_actions::PageActionModelInterface* GetObservedModel(
      actions::ActionId action_id) const;
  page_actions::PageActionController* GetController(
      actions::ActionId action_id);

  // Returns true if there is an anchored message currently showing for
  // `action_id`.
  bool IsAnchoredMessageShowing(actions::ActionId action_id) const;

  // Returns the AnchoredMessageBubbleView currently showing for `action_id`,
  // or nullptr if none is showing. For testing.
  AnchoredMessageBubbleView* GetAnchoredMessageForTesting(
      actions::ActionId action_id);

 private:
  // The internal implementation of PageActionController::Delegate and
  // PageActionModelObserver, held per ActionId in `delegates_`.
  class WebUIPageActionDelegate;

  // Plumbing method that calls OnPageActionChanged on `webui_delegate_`. Called
  // by the internal WebUIPageActionDelegate.
  void NotifyPageActionStateChanged();

  // Plumbing method that calls AnnounceAlert on `webui_delegate_`. Called by
  // the internal WebUIPageActionDelegate.
  void AnnounceAlert(const std::u16string& announcement);

  void OnControllerDestroying(page_actions::PageActionController& controller);

  // Advances `tab_switch_token_` and updates cached URL specs if `web_contents`
  // has changed either its visible URL (e.g. pending navigation start) or its
  // last committed URL. Returns true if the token advanced.
  bool MaybeAdvanceTabSwitchTokenForUrlChange(
      content::WebContents& web_contents);

  // Checks whether the active tab's URL changed before a delegate recomputes
  // its `PageActionState` in response to a model mutation, advancing
  // `tab_switch_token_` so navigation-driven icon updates do not push a stale
  // token.
  void MaybeAdvanceTabSwitchTokenForActiveTab();

  // Safe because the Browser window (which owns the ActionItem tree) owns
  // WebUILocationBar, which owns this control.
  const raw_ptr<actions::ActionItem> root_action_item_;

  // Safe because WebUIToolbarWebView (which implements this delegate) owns
  // WebUILocationBar (which owns this control).
  raw_ptr<WebUIToolbarControlDelegate> webui_delegate_ = nullptr;

  // Pointers to the active tab and its controller. Updated when the active tab
  // changes. `active_controller_` is owned by TabFeatures, which notifies
  // `OnControllerDestroying()` before either TabFeatures or TabInterface is
  // destroyed, resetting both pointers via `UpdateController(nullptr)`.
  raw_ptr<tabs::TabInterface> active_tab_ = nullptr;
  raw_ptr<page_actions::PageActionController> active_controller_ = nullptr;
  base::CallbackListSubscription active_controller_subscription_;

  std::map<actions::ActionId, std::unique_ptr<WebUIPageActionDelegate>>
      delegates_;
  std::map<actions::ActionId, std::unique_ptr<WebUIPageActionView>> views_;

  // Incremented on active controller (tab) changes and navigations. Used to
  // track active tab changes and propagate a token to WebUI to suppress
  // tab-switching icon animations and synchronize chip state.
  // TODO(crbug.com/581441485): Rename tab_switch_token to tab_update_token
  // across C++, Mojom, and TypeScript.
  uint32_t tab_switch_token_ = 0;

  // The visible and last-committed URL specs of the active tab's WebContents,
  // used to detect both pending navigation starts (e.g. BookmarkTabHelper
  // updating star state on DidStartNavigation via GetVisibleURL()) and
  // navigation commits so page-load icon changes suppress animations.
  std::string last_visible_url_spec_;
  std::string last_committed_url_spec_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_WEBUI_PAGE_ACTION_CONTROL_H_
