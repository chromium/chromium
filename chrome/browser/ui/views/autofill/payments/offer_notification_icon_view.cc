// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/view_class_properties.h"

namespace autofill {
constexpr double kAnimationValueWhenLabelFullyShown = 0.5;
constexpr base::TimeDelta kLabelPersistDuration = base::Seconds(10.8);

OfferNotificationIconView::OfferNotificationIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_OFFERS_AND_REWARDS_FOR_PAGE,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "PaymentsOfferNotification") {
  SetUpForInOutAnimation();
  SetProperty(views::kElementIdentifierKey, kOfferNotificationChipElementId);
  SetAccessibilityProperties(
      /*role*/ absl::nullopt,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT));
}

OfferNotificationIconView::~OfferNotificationIconView() = default;

views::BubbleDialogDelegate* OfferNotificationIconView::GetBubble() const {
  OfferNotificationBubbleController* controller = GetController();
  if (!controller)
    return nullptr;

  return static_cast<autofill::OfferNotificationBubbleViews*>(
      controller->GetOfferNotificationBubbleView());
}

void OfferNotificationIconView::UpdateImpl() {
  if (!GetWebContents())
    return;

  // |controller| may be nullptr due to lazy initialization.
  OfferNotificationBubbleController* controller = GetController();

  bool command_enabled =
      SetCommandEnabled(controller && controller->IsIconVisible());

  if (command_enabled) {
    MaybeShowPageActionLabel();
  } else {
    HidePageActionLabel();
  }
  SetVisible(command_enabled);
}

void OfferNotificationIconView::MaybeShowPageActionLabel() {
  OfferNotificationBubbleController* controller = GetController();
  if (!controller || !controller->ShouldIconExpand()) {
    return;
  }
  should_extend_label_shown_duration_ = true;
  SetPaintLabelOverSolidBackground(true);
  AnimateIn(IDS_DISCOUNT_ICON_EXPANDED_TEXT);
  controller->OnIconExpanded();
  SetAccessibilityProperties(
      /*role*/ absl::nullopt,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT));
}

void OfferNotificationIconView::HidePageActionLabel() {
  UnpauseAnimation();
  ResetSlideAnimation(false);
}

void OfferNotificationIconView::AnimationProgressed(
    const gfx::Animation* animation) {
  PageActionIconView::AnimationProgressed(animation);
  // When the label is fully revealed, pause the animation for
  // kLabelPersistDuration before resuming the animation and allowing the label
  // to animate out. This is currently set to show for 12s including the in/out
  // animation.
  // TODO(crbug.com/1314206): This approach of inspecting the animation progress
  // to extend the animation duration is quite hacky. This should be removed and
  // the IconLabelBubbleView API expanded to support a finer level of control.
  if (should_extend_label_shown_duration_ &&
      GetAnimationValue() >= kAnimationValueWhenLabelFullyShown) {
    should_extend_label_shown_duration_ = false;
    PauseAnimation();
    animate_out_timer_.Start(
        FROM_HERE, kLabelPersistDuration,
        base::BindRepeating(&OfferNotificationIconView::UnpauseAnimation,
                            base::Unretained(this)));
  }
}

void OfferNotificationIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& OfferNotificationIconView::GetVectorIcon() const {
  return OmniboxFieldTrial::IsChromeRefreshIconsEnabled()
             ? kLocalOfferFlippedRefreshIcon
             : kLocalOfferFlippedIcon;
}

const std::u16string& OfferNotificationIconView::GetIconLabelForTesting()
    const {
  return label()->GetText();
}

OfferNotificationBubbleController* OfferNotificationIconView::GetController()
    const {
  return OfferNotificationBubbleController::Get(GetWebContents());
}

BEGIN_METADATA(OfferNotificationIconView, PageActionIconView)
END_METADATA

}  // namespace autofill
