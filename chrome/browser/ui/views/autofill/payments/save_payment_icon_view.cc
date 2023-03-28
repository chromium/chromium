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
#include "chrome/browser/ui/views/autofill/payments/manage_saved_iban_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_iban_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"

namespace autofill {

SavePaymentIconView::SavePaymentIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    int command_id)
    : PageActionIconView(command_updater,
                         command_id,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         command_id == IDC_SAVE_CREDIT_CARD_FOR_PAGE
                             ? "SaveCard"
                             : "SaveIban") {
  if (command_id == IDC_SAVE_CREDIT_CARD_FOR_PAGE) {
    SetID(VIEW_ID_SAVE_CREDIT_CARD_BUTTON);
  } else {
    DCHECK(command_id == IDC_SAVE_IBAN_FOR_PAGE);
    SetID(VIEW_ID_SAVE_IBAN_BUTTON);
  }
  command_id_ = command_id;
  SetUpForInOutAnimation();
  SetAccessibilityProperties(/*role*/ absl::nullopt,
                             GetTextForTooltipAndAccessibleName());
}

SavePaymentIconView::~SavePaymentIconView() = default;

views::BubbleDialogDelegate* SavePaymentIconView::GetBubble() const {
  SavePaymentIconController* controller = GetController();
  if (!controller)
    return nullptr;

  switch (controller->GetPaymentBubbleType()) {
    case SavePaymentIconController::PaymentBubbleType::kUnknown:
      return nullptr;
    case SavePaymentIconController::PaymentBubbleType::kCreditCard:
      return static_cast<autofill::SaveCardBubbleViews*>(
          controller->GetPaymentBubbleView());
    case SavePaymentIconController::PaymentBubbleType::kSaveIban:
      return static_cast<autofill::SaveIbanBubbleView*>(
          controller->GetPaymentBubbleView());
    case SavePaymentIconController::PaymentBubbleType::kManageSavedIban:
      return static_cast<autofill::ManageSavedIbanBubbleView*>(
          controller->GetPaymentBubbleView());
  }
}

void SavePaymentIconView::UpdateImpl() {
  if (!GetWebContents())
    return;

  // |controller| may be nullptr due to lazy initialization.
  SavePaymentIconController* controller = GetController();

  bool command_enabled =
      SetCommandEnabled(controller && controller->IsIconVisible());
  SetVisible(command_enabled);

  SetAccessibleName(GetTextForTooltipAndAccessibleName());

  if (command_enabled && controller->ShouldShowSavingPaymentAnimation()) {
    SetEnabled(false);
    SetIsLoading(/*is_loading=*/true);
  } else {
    SetIsLoading(/*is_loading=*/false);
    UpdateIconImage();
    SetEnabled(true);
  }

  if (command_enabled && controller->ShouldShowPaymentSavedLabelAnimation()) {
    if (command_id_ == IDC_SAVE_CREDIT_CARD_FOR_PAGE) {
      AnimateIn(IDS_AUTOFILL_CARD_SAVED);
    } else if (command_id_ == IDC_SAVE_IBAN_FOR_PAGE) {
      AnimateIn(IDS_AUTOFILL_IBAN_SAVED);
    }
  }
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
  return SavePaymentIconController::Get(GetWebContents(), command_id_);
}

void SavePaymentIconView::AnimationEnded(const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationEnded(animation);

  // |controller| may be nullptr due to lazy initialization.
  SavePaymentIconController* controller = GetController();
  if (controller)
    controller->OnAnimationEnded();
}

BEGIN_METADATA(SavePaymentIconView, PageActionIconView)
END_METADATA

}  // namespace autofill
