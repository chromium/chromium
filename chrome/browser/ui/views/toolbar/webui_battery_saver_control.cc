// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_battery_saver_control.h"

#include "base/check.h"
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

void WebUIBatterySaverControl::ShowBubble(gfx::Rect anchor_rect) {
  if (bubble_) {
    BatterySaverBubbleView::CloseBubble(bubble_);
  } else {
    CloseFeaturePromo(/*engaged=*/true);

    ui::TrackedElement* button_element =
        BrowserElements::From(delegate_->GetBrowser())
            ->GetElement(kToolbarBatterySaverButtonElementId);
    DCHECK(button_element);

    bubble_ = BatterySaverBubbleView::CreateBubble(
        views::BubbleAnchor(button_element), views::BubbleBorder::TOP_RIGHT,
        this, anchor_rect);
  }
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
}

toolbar_ui_api::mojom::BatterySaverControlStatePtr
WebUIBatterySaverControl::CreateState() const {
  auto state = toolbar_ui_api::mojom::BatterySaverControlState::New();
  state->should_be_shown = is_showing_;
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
