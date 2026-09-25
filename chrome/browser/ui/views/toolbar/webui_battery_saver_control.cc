// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_battery_saver_control.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/performance_controls/battery_saver_bubble_delegate.h"
#include "chrome/browser/ui/performance_controls/battery_saver_bubble_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/performance_controls/battery_saver_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

WebUIBatterySaverControl::WebUIBatterySaverControl(
    WebUIToolbarControlDelegate* delegate)
    : delegate_(delegate),
      is_showing_(features::IsWebUIBatterySaverButtonEnabled() &&
                  performance_manager::user_tuning::BatterySaverModeManager::
                      HasInstance() &&
                  performance_manager::user_tuning::BatterySaverModeManager::
                      GetInstance()
                          ->IsBatterySaverActive()) {}

WebUIBatterySaverControl::~WebUIBatterySaverControl() {
  if (bubble_) {
    BatterySaverBubbleView::CloseBubble(bubble_);
  }
}

void WebUIBatterySaverControl::Init() {
  controller_.Init(this);
}

void WebUIBatterySaverControl::ShowBubble(
    std::optional<gfx::Rect> anchor_rect) {
  // Consider this call to fulfill any pending attempt to show the bubble,
  // though if the button still isn't visible, it will subscribe again below.
  //
  // Clearing `pending_show_bubble_` here can change `prevent_overflow`, which
  // means the renderer needs to be told. Every path below calls UpdateState(),
  // except the `bubble_` case, which doesn't need to: `pending_show_bubble_` is
  // only ever set while `bubble_` is null, and is cleared before `bubble_` is
  // assigned, so the two are never true at once, and the clear above is a no-op
  // whenever there's a bubble.
  button_shown_subscription_ = {};
  pending_show_bubble_ = false;

  if (bubble_) {
    BatterySaverBubbleView::CloseBubble(bubble_);
    return;
  }

  BrowserElements* const elements =
      BrowserElements::From(delegate_->GetBrowser());
  ui::TrackedElement* element =
      elements->GetElement(kToolbarBatterySaverButtonElementId);

  // If the button is hidden, have to set `pending_show_bubble_` to true and
  // subscribe to a notification for when it is visible.
  if (!element) {
    pending_show_bubble_ = true;
    button_shown_subscription_ =
        ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
            kToolbarBatterySaverButtonElementId, elements->GetContext(),
            base::BindRepeating(
                &WebUIBatterySaverControl::OnButtonShownWithPendingShowBubble,
                base::Unretained(this)));
    // Need to push the updated state, which now should have `prevent_overflow`
    // set. That will result in the button element being unconditionally shown
    // until the bubble is hidden. Window shouldn't be shrinkable while there's
    // a bubble, anyways, but best to keep state consistent.
    UpdateState();
    return;
  }

  // Note that this is deliberately not done above, when the button is still
  // hidden: this method is invoked a second time once the button becomes
  // visible, and notifying that the feature was used twice per click would
  // double-count it in the feature engagement backend.
  CloseFeaturePromo(/*engaged=*/true);

  // This calls `element->GetScreenBounds()` unnecessarily when `anchor_rect` is
  // set, but it's not worth optimizing that out.
  bubble_ = BatterySaverBubbleView::CreateBubble(
      views::BubbleAnchor(element), views::BubbleBorder::TOP_RIGHT, this,
      anchor_rect.value_or(element->GetScreenBounds()));

  // Since `bubble_` has changed, need to call UpdateState() to pass the updated
  // value of `prevent_overflow` to the renderer.
  UpdateState();
}

void WebUIBatterySaverControl::Show() {
  if (is_showing_) {
    return;
  }
  is_showing_ = true;
  UpdateState();
  delegate_->OnPreferredSizeChanged();

  // Try to show feature promo
  BrowserUserEducationInterface::From(delegate_->GetBrowser())
      ->MaybeShowFeaturePromo(feature_engagement::kIPHBatterySaverModeFeature);
}

void WebUIBatterySaverControl::Hide() {
  CloseFeaturePromo(/*engaged=*/false);
  CancelPendingShowBubble();
  if (bubble_) {
    BatterySaverBubbleView::CloseBubble(bubble_);
  }
  if (!is_showing_) {
    return;
  }
  is_showing_ = false;
  UpdateState();
  delegate_->OnPreferredSizeChanged();
}

void WebUIBatterySaverControl::OnBubbleShown() {}

void WebUIBatterySaverControl::OnBubbleHidden() {
  bubble_ = nullptr;
  // Let the WebUI know the button no longer needs to be displayed.
  UpdateState();
}

toolbar_ui_api::mojom::BatterySaverControlStatePtr
WebUIBatterySaverControl::CreateState() const {
  auto state = toolbar_ui_api::mojom::BatterySaverControlState::New();
  state->should_be_shown = is_showing_;
  if (is_showing_) {
    // Note that this is set while the bubble is merely pending, to cause the
    // button to be displayed so the bubble can then be anchored to it.
    state->prevent_overflow = pending_show_bubble_ || bubble_ != nullptr;
  }
  return state;
}

void WebUIBatterySaverControl::UpdateState() {
  delegate_->OnBatterySaverControlStateChanged(CreateState());
}

void WebUIBatterySaverControl::CloseFeaturePromo(bool engaged) {
  if (engaged) {
    BrowserUserEducationInterface::From(delegate_->GetBrowser())
        ->NotifyFeaturePromoFeatureUsed(
            feature_engagement::kIPHBatterySaverModeFeature,
            FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  } else {
    BrowserUserEducationInterface::From(delegate_->GetBrowser())
        ->AbortFeaturePromo(feature_engagement::kIPHBatterySaverModeFeature);
  }
}

void WebUIBatterySaverControl::OnButtonShownWithPendingShowBubble(
    ui::TrackedElement* element) {
  // This call should have been cancelled if `pending_show_bubble_` became false
  // before it was invoked.
  DCHECK(pending_show_bubble_);
  // ShowBubble() will clear `pending_show_bubble_`, and is guaranteed to find
  // `element` when it looks for it, so we don't get into an infinite loop
  // trying to show the bubble and not finding the TrackedElement.
  ShowBubble();
}

void WebUIBatterySaverControl::CancelPendingShowBubble() {
  button_shown_subscription_ = {};
  if (std::exchange(pending_show_bubble_, false)) {
    UpdateState();
  }
}
