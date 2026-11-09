// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_IPH_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "content/public/browser/global_dom_node_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"

class BrowserUserEducationInterface;

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace content {
struct FocusedNodeDetails;
class Page;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace views {
class View;
class ViewSubregionAnchor;
}  // namespace views

namespace dictation {

// Tab-scoped controller that shows the dictation IPH next to a medium or large
// editable field when the user focuses it with the mouse or keyboard. Users who
// have already completed dictation onboarding or have no microphone never see
// it.
//
// The IPH is anchored to a `views::ViewSubregionAnchor` over the focused field
// in the tab's `ContentsWebView`. The IPH is closed as "ignored" when the user
// types a few keys, moves to another field, focus leaves the field, the tab is
// deactivated, the page navigates, or the frame holding the field goes away
// (e.g. the field's iframe navigates or is removed).
class DictationIphController : public tabs::ContentsObservingTabFeature {
 public:
  explicit DictationIphController(tabs::TabInterface& tab);
  DictationIphController(const DictationIphController&) = delete;
  DictationIphController& operator=(const DictationIphController&) = delete;
  ~DictationIphController() override;

  // content::WebContentsObserver:
  void OnFocusChangedInPage(
      const content::FocusedNodeDetails& details) override;
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;
  void PrimaryPageChanged(content::Page& page) override;
  void RenderFrameHostStateChanged(
      content::RenderFrameHost* render_frame_host,
      content::RenderFrameHost::LifecycleState old_state,
      content::RenderFrameHost::LifecycleState new_state) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

 private:
  void OnTabWillDeactivate(tabs::TabInterface* tab);

  // Returns the `ContentsWebView` currently displaying this tab, or nullptr.
  views::View* GetContentsWebView();

  // Returns the user education interface for this tab's window, or nullptr.
  BrowserUserEducationInterface* GetUserEducation() const;

  // Returns whether the dictation IPH is currently showing.
  bool IsPromoActive() const;

  // Returns whether the user has already used dictation (completed onboarding).
  // Such users are not shown the IPH.
  bool HasUsedDictation() const;

  // Returns whether `node` is the field the anchor is currently tracking.
  bool IsAnchoredNode(const content::GlobalDOMNodeId& node) const;

  // Closes the IPH, or cancels it if it is queued, because the user ignored
  // it: they typed in the field, left the field, navigated, or switched tabs.
  void IgnorePromo();

  // Closes the IPH as ignored and destroys the anchor.
  void HideAnchor();

  // Checks whether the anchored field is a medium or large text field. If it
  // isn't, hides the anchor. If it is and `can_start_promo` is true, shows the
  // IPH. If the field's type isn't known yet, retries up to `retries_left` more
  // times.
  void MaybeShowPromo(int retries_left, bool can_start_promo);

  // Tracks the focused field. Only exists while a field is focused.
  std::unique_ptr<views::ViewSubregionAnchor> anchor_;
  // The field `anchor_` is tracking.
  std::optional<content::GlobalDOMNodeId> anchored_node_;
  // The frame holding `anchored_node_`. The anchor is hidden when this frame
  // stops being the active document or is deleted, which also covers fields in
  // iframes.
  content::GlobalRenderFrameHostId anchored_frame_id_;
  // Number of keys typed while the IPH is showing.
  int key_presses_while_showing_ = 0;
  // Delays hiding the anchor when focus leaves a field; see
  // `OnFocusChangedInPage()`.
  base::OneShotTimer hide_timer_;
  // Retries the medium/large field check; see `MaybeShowPromo()`.
  base::OneShotTimer field_type_retry_timer_;
  base::CallbackListSubscription tab_will_deactivate_subscription_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_IPH_CONTROLLER_H_
