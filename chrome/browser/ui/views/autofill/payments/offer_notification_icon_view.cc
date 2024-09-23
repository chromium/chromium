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
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"

namespace autofill {
constexpr double kAnimationValueWhenLabelFullyShown = 0.5;
constexpr base::TimeDelta kLabelPersistDuration = base::Seconds(10.8);

DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kLabelAnimationFinished);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kLabelExpansionFinished);

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
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT));
}

OfferNotificationIconView::~OfferNotificationIconView() = default;

views::BubbleDialogDelegate* OfferNotificationIconView::GetBubble() const {
  OfferNotificationBubbleController* controller = GetController();
  if (!controller) {
    return nullptr;
  }

  return static_cast<autofill::OfferNotificationBubbleViews*>(
      controller->GetOfferNotificationBubbleView());
}

void OfferNotificationIconView::UpdateImpl() {
  if (!GetWebContents()) {
    return;
  }

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

void OfferNotificationIconView::OnWidgetDestroying(views::Widget* widget) {
  CHECK(bubble_widget_observation_.IsObservingSource(widget));
  bubble_widget_observation_.Reset();
  UnpauseAnimation();
}

void OfferNotificationIconView::MaybeShowPageActionLabel() {
  OfferNotificationBubbleController* controller = GetController();
  if (!controller || !controller->ShouldIconExpand()) {
    return;
  }
  should_extend_label_shown_duration_ = true;
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);
  AnimateIn(IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT);
  controller->OnIconExpanded();
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT));
}

void OfferNotificationIconView::HidePageActionLabel() {
  UnpauseAnimation();
  ResetSlideAnimation(false);
}

base::RetainingOneShotTimer& OfferNotificationIconView::AnimateOutTimer() {
  return animate_out_timer_for_testing_ ? *animate_out_timer_for_testing_
                                        : animate_out_timer_;
}

void OfferNotificationIconView::AnimationProgressed(
    const gfx::Animation* animation) {
  PageActionIconView::AnimationProgressed(animation);
  // When the label is fully revealed, pause the animation for
  // kLabelPersistDuration before resuming the animation and allowing the label
  // to animate out. This is currently set to show for 12s including the in/out
  // animation.
  // TODO(crbug.com/40832707): This approach of inspecting the animation
  // progress to extend the animation duration is quite hacky. This should be
  // removed and the IconLabelBubbleView API expanded to support a finer level
  // of control.
  if (should_extend_label_shown_duration_ &&
      GetAnimationValue() >= kAnimationValueWhenLabelFullyShown) {
    should_extend_label_shown_duration_ = false;
    PauseAnimation();

    auto* bubble = GetBubble();
    if (bubble) {
      bubble_widget_observation_.Observe(bubble->GetWidget());
    } else {
      AnimateOutTimer().Start(
          FROM_HERE, kLabelPersistDuration,
          base::BindRepeating(&OfferNotificationIconView::UnpauseAnimation,
                              base::Unretained(this)));
    }
    views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
        kLabelExpansionFinished, this);
  }
}

void OfferNotificationIconView::AnimationEnded(
    const gfx::Animation* animation) {
  PageActionIconView::AnimationEnded(animation);

  views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
      kLabelAnimationFinished, this);
}

void OfferNotificationIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  if (AnimateOutTimer().IsRunning()) {
    AnimateOutTimer().Stop();
  }
}

void OfferNotificationIconView::DidExecute(ExecuteSource execute_source) {
  auto* bubble = GetBubble();
  CHECK(bubble);
  bubble_widget_observation_.Observe(bubble->GetWidget());
}

const gfx::VectorIcon& OfferNotificationIconView::GetVectorIcon() const {
  return kLocalOfferFlippedRefreshIcon;
}

const std::u16string& OfferNotificationIconView::GetIconLabelForTesting()
    const {
  return label()->GetText();
}

void OfferNotificationIconView::SetAnimateOutTimerForTesting(
    base::RetainingOneShotTimer* timer) {
  animate_out_timer_for_testing_ = timer;
}

OfferNotificationBubbleController* OfferNotificationIconView::GetController()
    const {
  return OfferNotificationBubbleController::Get(GetWebContents());
}

BEGIN_METADATA(OfferNotificationIconView)
END_METADATA

}  // namespace autofill
