// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"

#include "base/notreached.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/payments/save_payment_icon_controller.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "chrome/browser/ui/views/autofill/payments/manage_saved_iban_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_iban_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_method_and_virtual_card_enroll_confirmation_bubble_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace autofill {

SavePaymentIconView::SavePaymentIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    int command_id)
    : PageActionIconView(
          command_updater,
          command_id,
          icon_label_bubble_delegate,
          page_action_icon_delegate,
          command_id == IDC_SAVE_CREDIT_CARD_FOR_PAGE ? "SaveCard" : "SaveIban",
          kActionShowPaymentsBubbleOrPage) {
  if (command_id == IDC_SAVE_CREDIT_CARD_FOR_PAGE) {
    SetID(VIEW_ID_SAVE_CREDIT_CARD_BUTTON);
  } else {
    DCHECK(command_id == IDC_SAVE_IBAN_FOR_PAGE);
    SetID(VIEW_ID_SAVE_IBAN_BUTTON);
  }
  command_id_ = command_id;
  SetUpForInOutAnimation();
  GetViewAccessibility().SetName(GetTextForTooltipAndAccessibleName());
}

SavePaymentIconView::~SavePaymentIconView() = default;

views::BubbleDialogDelegate* SavePaymentIconView::GetBubble() const {
  return GetController() ? static_cast<AutofillLocationBarBubble*>(
                               GetController()->GetPaymentBubbleView())
                         : nullptr;
}

void SavePaymentIconView::UpdateImpl() {
  if (!GetWebContents()) {
    return;
  }

  // |controller| may be nullptr due to lazy initialization.
  SavePaymentIconController* controller = GetController();

  bool command_enabled =
      SetCommandEnabled(controller && controller->IsIconVisible());
  const bool should_show =
      command_enabled && !delegate()->ShouldHidePageActionIcon(this);
  SetVisible(should_show);

  GetViewAccessibility().SetName(GetTextForTooltipAndAccessibleName());

  if (command_enabled && controller->ShouldShowSavingPaymentAnimation()) {
    SetEnabled(false);
    SetIsLoading(/*is_loading=*/true);
  } else {
    SetIsLoading(/*is_loading=*/false);
    UpdateIconImage();
    SetEnabled(true);
  }

  if (command_enabled && controller->ShouldShowPaymentSavedLabelAnimation()) {
    AnimateIn(controller->GetSaveSuccessAnimationStringId());
  }
}

void SavePaymentIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& SavePaymentIconView::GetVectorIcon() const {
  return kCreditCardChromeRefreshIcon;
}

std::u16string SavePaymentIconView::GetTextForTooltipAndAccessibleName() const {
  std::u16string text;

  SavePaymentIconController* const controller = GetController();
  if (controller) {
    text = controller->GetSavePaymentIconTooltipText();
  }

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
  if (controller) {
    controller->OnAnimationEnded();
  }
}

BEGIN_METADATA(SavePaymentIconView)
END_METADATA

}  // namespace autofill
