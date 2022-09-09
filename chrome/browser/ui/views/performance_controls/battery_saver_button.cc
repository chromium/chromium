// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/battery_saver_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/performance_controls/battery_saver_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/view_class_properties.h"

BatterySaverButton::BatterySaverButton(BrowserView* browser_view)
    : ToolbarButton(base::BindRepeating(&BatterySaverButton::OnClicked,
                                        base::Unretained(this))),
      browser_view_(browser_view) {
  SetVectorIcon(kBatterySaverIcon);
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);

  // Do not flip the battery saver icon for RTL languages.
  SetFlipCanvasOnPaintForRTLUI(false);

  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_BUTTON_ACCNAME));
  SetTooltipText(l10n_util::GetStringUTF16(IDS_BATTERY_SAVER_BUTTON_TOOLTIP));
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kDialog);
  SetProperty(views::kElementIdentifierKey, kBatterySaverButtonElementId);

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

  if (!was_visible)
    MaybeShowFeaturePromo();
}

void BatterySaverButton::Hide() {
  if (IsBubbleShowing()) {
    // The bubble is closed sync and will be cleared in OnBubbleHidden
    BatterySaverBubbleView::CloseBubble(bubble_);
  }

  SetVisible(false);
}

void BatterySaverButton::OnBubbleHidden() {
  bubble_ = nullptr;
}

void BatterySaverButton::OnClicked() {
  if (IsBubbleShowing()) {
    // The bubble is closed sync and will be cleared in OnBubbleHidden
    BatterySaverBubbleView::CloseBubble(bubble_);
  } else {
    CloseFeaturePromo();

    browser_view_->NotifyFeatureEngagementEvent(
        feature_engagement::events::kBatterySaverDialogShown);

    bubble_ = BatterySaverBubbleView::CreateBubble(
        browser_view_->browser(), this, views::BubbleBorder::TOP_RIGHT, this);
  }
}

void BatterySaverButton::OnFeatureEngagementInitialized(bool initialized) {
  if (!initialized)
    return;

  browser_view_->MaybeShowFeaturePromo(
      feature_engagement::kIPHBatterySaverModeFeature);
}

void BatterySaverButton::MaybeShowFeaturePromo() {
  auto* const promo_controller = browser_view_->GetFeaturePromoController();
  if (!promo_controller)
    return;

  // Toolbar button could be visible early in browser startup where the feature
  // engagement tracker might not have fully initialized. So wait for the
  // initialization to complete before triggering the promo.
  auto* tracker = promo_controller->feature_engagement_tracker();
  tracker->AddOnInitializedCallback(
      base::BindOnce(&BatterySaverButton::OnFeatureEngagementInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BatterySaverButton::CloseFeaturePromo() {
  // CloseFeaturePromo checks if the promo is active for the feature before
  // attempting to close the promo bubble
  browser_view_->CloseFeaturePromo(
      feature_engagement::kIPHBatterySaverModeFeature);
}

BEGIN_METADATA(BatterySaverButton, ToolbarButton)
END_METADATA
