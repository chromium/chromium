// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/battery_saver_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/performance_controls/battery_saver_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/view_class_properties.h"

BatterySaverButton::BatterySaverButton(BrowserView* browser_view)
    : ToolbarButton(base::BindRepeating(&BatterySaverButton::OnClicked,
                                        base::Unretained(this))),
      browser_view_(browser_view) {
  SetVectorIcon(kBatterySaverRefreshIcon);
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);

  // Do not flip the battery saver icon for RTL languages.
  SetFlipCanvasOnPaintForRTLUI(false);

  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_BUTTON_ACCNAME));
  SetTooltipText(l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_BUTTON_TOOLTIP));
  GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kDialog);
  SetProperty(views::kElementIdentifierKey,
              kToolbarBatterySaverButtonElementId);

  // We start hidden and only show once |controller_| tells us to.
  SetVisible(false);

  // Initialize the |controller_| which will update the correct visible state.
  controller_.Init(this);
}

BatterySaverButton::~BatterySaverButton() {
  if (IsBubbleShowing())
    BatterySaverBubbleView::CloseBubble(bubble_);
}

bool BatterySaverButton::IsBubbleShowing() const {
  return bubble_ != nullptr;
}

void BatterySaverButton::Show() {
  bool was_visible = GetVisible();
  SetVisible(true);
  PreferredSizeChanged();

  // Wait until the view is properly laid out before triggering the promo
  // The promo will be triggered in |OnBoundsChanged| if the flag is set
  if (!was_visible)
    pending_promo_ = true;
}

void BatterySaverButton::Hide() {
  CloseFeaturePromo(/*engaged=*/false);

  if (IsBubbleShowing()) {
    // The bubble is closed sync and will be cleared in OnBubbleHidden
    BatterySaverBubbleView::CloseBubble(bubble_);
  }

  SetVisible(false);
  PreferredSizeChanged();
}

void BatterySaverButton::OnBubbleHidden() {
  bubble_ = nullptr;
}

bool BatterySaverButton::ShouldShowInkdropAfterIphInteraction() {
  return false;
}

void BatterySaverButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  ToolbarButton::OnBoundsChanged(previous_bounds);

  if (!GetVisible() || size().IsEmpty())
    return;

  if (pending_promo_)
    MaybeShowFeaturePromo();
}

void BatterySaverButton::OnClicked() {
  if (IsBubbleShowing()) {
    // The bubble is closed sync and will be cleared in OnBubbleHidden
    BatterySaverBubbleView::CloseBubble(bubble_);
  } else {
    CloseFeaturePromo(/*engaged=*/true);
    bubble_ = BatterySaverBubbleView::CreateBubble(
        browser_view_->browser(), this, views::BubbleBorder::TOP_RIGHT, this);
  }
}

void BatterySaverButton::MaybeShowFeaturePromo() {
  pending_promo_ = false;
  browser_view_->MaybeShowStartupFeaturePromo(
      feature_engagement::kIPHBatterySaverModeFeature);
}

void BatterySaverButton::CloseFeaturePromo(bool engaged) {
  // CloseFeaturePromo checks if the promo is active for the feature before
  // attempting to close the promo bubble
  pending_promo_ = false;
  if (engaged) {
    browser_view_->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHBatterySaverModeFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  } else {
    browser_view_->AbortFeaturePromo(
        feature_engagement::kIPHBatterySaverModeFeature);
  }
}

BEGIN_METADATA(BatterySaverButton)
END_METADATA
