// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_card_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace autofill {

SaveCardIconView::SaveCardIconView(CommandUpdater* command_updater,
                                   PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater,
                         IDC_SAVE_CREDIT_CARD_FOR_PAGE,
                         delegate) {
  DCHECK(delegate);
  SetID(VIEW_ID_SAVE_CREDIT_CARD_BUTTON);

  if (base::FeatureList::IsEnabled(
          features::kAutofillCreditCardUploadFeedback)) {
    InstallLoadingIndicator();
  }
  SetUpForInOutAnimation();
}

SaveCardIconView::~SaveCardIconView() = default;

views::BubbleDialogDelegateView* SaveCardIconView::GetBubble() const {
  SaveCardBubbleController* controller = GetController();
  if (!controller)
    return nullptr;

  return static_cast<autofill::SaveCardBubbleViews*>(
      controller->GetSaveCardBubbleView());
}

bool SaveCardIconView::Update() {
  if (!GetWebContents())
    return false;

  const bool was_visible = GetVisible();

  // |controller| may be nullptr due to lazy initialization.
  SaveCardBubbleController* controller = GetController();

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

  return was_visible != GetVisible();
}

void SaveCardIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& SaveCardIconView::GetVectorIcon() const {
  return kCreditCardIcon;
}

const gfx::VectorIcon& SaveCardIconView::GetVectorIconBadge() const {
  SaveCardBubbleController* controller = GetController();
  if (controller && controller->ShouldShowSaveFailureBadge())
    return kBlockedBadgeIcon;

  return gfx::kNoneIcon;
}

base::string16 SaveCardIconView::GetTextForTooltipAndAccessibleName() const {
  SaveCardBubbleController* controller = GetController();
  if (!controller) {
    // The controller can be null in unit tests only.
    return base::string16();
  }

  return controller->GetSaveCardIconTooltipText();
}

SaveCardBubbleController* SaveCardIconView::GetController() const {
  return SaveCardBubbleController::Get(GetWebContents());
}

void SaveCardIconView::AnimationEnded(const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationEnded(animation);

  // |controller| may be nullptr due to lazy initialization.
  SaveCardBubbleController* controller = GetController();
  if (controller)
    controller->OnAnimationEnded();
}

}  // namespace autofill
