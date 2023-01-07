// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/save_payment_icon_controller.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace autofill {

SavePaymentIconView::SavePaymentIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_SAVE_CREDIT_CARD_FOR_PAGE,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "SaveCard") {
  SetID(VIEW_ID_SAVE_CREDIT_CARD_BUTTON);
  SetUpForInOutAnimation();
}

SavePaymentIconView::~SavePaymentIconView() = default;

views::BubbleDialogDelegate* SavePaymentIconView::GetBubble() const {
  SavePaymentIconController* controller = GetController();
  if (!controller)
    return nullptr;

  return static_cast<autofill::SaveCardBubbleViews*>(
      controller->GetSaveBubbleView());
}

void SavePaymentIconView::UpdateImpl() {
  if (!GetWebContents())
    return;

  // |controller| may be nullptr due to lazy initialization.
  SavePaymentIconController* controller = GetController();

  bool command_enabled =
      SetCommandEnabled(controller && controller->IsIconVisible());
  SetVisible(command_enabled);

  if (command_enabled && controller->ShouldShowSavingCardAnimation()) {
    SetEnabled(false);
    SetIsLoading(/*is_loading=*/true);
  } else {
    SetIsLoading(/*is_loading=*/false);
    UpdateIconImage();
    SetEnabled(true);
  }

  if (command_enabled && controller->ShouldShowCardSavedLabelAnimation())
    AnimateIn(IDS_AUTOFILL_CARD_SAVED);
}

void SavePaymentIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& SavePaymentIconView::GetVectorIcon() const {
  return kCreditCardIcon;
}

const gfx::VectorIcon& SavePaymentIconView::GetVectorIconBadge() const {
  SavePaymentIconController* controller = GetController();
  if (controller && controller->ShouldShowSaveFailureBadge())
    return vector_icons::kBlockedBadgeIcon;

  return gfx::kNoneIcon;
}

const char* SavePaymentIconView::GetClassName() const {
  return "SavePaymentIconView";
}

std::u16string SavePaymentIconView::GetTextForTooltipAndAccessibleName() const {
  std::u16string text;

  SavePaymentIconController* const controller = GetController();
  if (controller)
    text = controller->GetSavePaymentIconTooltipText();

  // Because the payment icon is in an animated container, it is still briefly
  // visible as it's disappearing. Since our test infrastructure does not allow
  // views to have empty tooltip text when they are visible, we instead return
  // the default text.
  return text.empty() ? l10n_util::GetStringUTF16(IDS_TOOLTIP_SAVE_CREDIT_CARD)
                      : text;
}

SavePaymentIconController* SavePaymentIconView::GetController() const {
  return SavePaymentIconController::Get(GetWebContents());
}

void SavePaymentIconView::AnimationEnded(const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationEnded(animation);

  // |controller| may be nullptr due to lazy initialization.
  SavePaymentIconController* controller = GetController();
  if (controller)
    controller->OnAnimationEnded();
}

}  // namespace autofill
