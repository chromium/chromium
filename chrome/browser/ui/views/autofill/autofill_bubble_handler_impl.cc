// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_bubble_handler_impl.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_view.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_failure_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_manage_cards_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_offer_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_sign_in_promo_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace autofill {

AutofillBubbleHandlerImpl::AutofillBubbleHandlerImpl(
    Browser* browser,
    ToolbarButtonProvider* toolbar_button_provider)
    : browser_(browser), toolbar_button_provider_(toolbar_button_provider) {
  if (browser->profile()) {
    personal_data_manager_observer_.Add(
        PersonalDataManagerFactory::GetForProfile(
            browser->profile()->GetOriginalProfile()));
  }
  if (toolbar_button_provider_->GetAvatarToolbarButton())
    avatar_toolbar_button_observer_.Add(
        toolbar_button_provider_->GetAvatarToolbarButton());
}

AutofillBubbleHandlerImpl::~AutofillBubbleHandlerImpl() = default;

// TODO(crbug.com/932818): Clean up this two functions and add helper for shared
// code.
SaveCardBubbleView* AutofillBubbleHandlerImpl::ShowSaveCreditCardBubble(
    content::WebContents* web_contents,
    SaveCardBubbleController* controller,
    bool is_user_gesture) {
  BubbleType bubble_type = controller->GetBubbleType();
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kSaveCard);
  views::View* anchor_view =
      toolbar_button_provider_->GetAnchorView(PageActionIconType::kSaveCard);

  SaveCardBubbleViews* bubble = nullptr;
  switch (bubble_type) {
    case BubbleType::LOCAL_SAVE:
    case BubbleType::UPLOAD_SAVE:
      bubble =
          new SaveCardOfferBubbleViews(anchor_view, web_contents, controller);
      break;
    case BubbleType::SIGN_IN_PROMO:
      DCHECK(!base::FeatureList::IsEnabled(
          features::kAutofillCreditCardUploadFeedback));
      bubble = new SaveCardSignInPromoBubbleViews(anchor_view, web_contents,
                                                  controller);
      break;
    case BubbleType::MANAGE_CARDS:
      bubble = new SaveCardManageCardsBubbleViews(anchor_view, web_contents,
                                                  controller);
      break;
    case BubbleType::FAILURE:
      bubble =
          new SaveCardFailureBubbleViews(anchor_view, web_contents, controller);
      break;
    case BubbleType::UPLOAD_IN_PROGRESS:
    case BubbleType::INACTIVE:
      break;
  }
  DCHECK(bubble);

  if (icon_view)
    bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(is_user_gesture ? SaveCardBubbleViews::USER_GESTURE
                               : SaveCardBubbleViews::AUTOMATIC);
  return bubble;
}

SaveCardBubbleView* AutofillBubbleHandlerImpl::ShowSaveCardSignInPromoBubble(
    content::WebContents* web_contents,
    SaveCardBubbleController* controller) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillCreditCardUploadFeedback));
  views::Button* avatar_button =
      toolbar_button_provider_->GetAvatarToolbarButton();
  DCHECK(avatar_button);

  SaveCardBubbleViews* bubble = new SaveCardSignInPromoBubbleViews(
      avatar_button, web_contents, controller);
  bubble->SetHighlightedButton(avatar_button);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(SaveCardBubbleViews::AUTOMATIC);
  return bubble;
}

LocalCardMigrationBubble*
AutofillBubbleHandlerImpl::ShowLocalCardMigrationBubble(
    content::WebContents* web_contents,
    LocalCardMigrationBubbleController* controller,
    bool is_user_gesture) {
  LocalCardMigrationBubbleViews* bubble = new LocalCardMigrationBubbleViews(
      toolbar_button_provider_->GetAnchorView(
          PageActionIconType::kLocalCardMigration),
      web_contents, controller);

  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kLocalCardMigration);
  if (icon_view)
    bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(is_user_gesture ? LocalCardMigrationBubbleViews::USER_GESTURE
                               : LocalCardMigrationBubbleViews::AUTOMATIC);
  return bubble;
}

void AutofillBubbleHandlerImpl::OnPasswordSaved() {
  if (base::FeatureList::IsEnabled(
          features::kAutofillCreditCardUploadFeedback)) {
    ShowAvatarHighlightAnimation();
  }
}

void AutofillBubbleHandlerImpl::HideSignInPromo() {
  chrome::ExecuteCommand(browser_, IDC_CLOSE_SIGN_IN_PROMO);
}

void AutofillBubbleHandlerImpl::OnCreditCardSaved(
    bool should_show_sign_in_promo_if_applicable) {
  should_show_sign_in_promo_if_applicable_ =
      should_show_sign_in_promo_if_applicable;
  ShowAvatarHighlightAnimation();
}

void AutofillBubbleHandlerImpl::OnAvatarHighlightAnimationFinished() {
  if (should_show_sign_in_promo_if_applicable_) {
    should_show_sign_in_promo_if_applicable_ = false;
    chrome::ExecuteCommand(
        browser_, IDC_SHOW_SAVE_LOCAL_CARD_SIGN_IN_PROMO_IF_APPLICABLE);
  }
}

void AutofillBubbleHandlerImpl::ShowAvatarHighlightAnimation() {
  AvatarToolbarButton* avatar =
      toolbar_button_provider_->GetAvatarToolbarButton();
  if (avatar)
    avatar->ShowAvatarHighlightAnimation();
}

}  // namespace autofill
