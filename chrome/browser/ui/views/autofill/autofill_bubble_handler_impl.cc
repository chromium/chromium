// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_bubble_handler_impl.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/autofill/payments/save_iban_ui.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/manage_saved_iban_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_confirmation_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_opt_in_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_failure_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_manage_cards_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_offer_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_iban_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_manual_fallback_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_manual_fallback_icon_view.h"
#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"
#include "chrome/browser/ui/views/autofill/update_address_profile_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace autofill {

AutofillBubbleHandlerImpl::AutofillBubbleHandlerImpl(
    Browser* browser,
    ToolbarButtonProvider* toolbar_button_provider)
    : browser_(browser), toolbar_button_provider_(toolbar_button_provider) {
  if (toolbar_button_provider_->GetAvatarToolbarButton()) {
    avatar_toolbar_button_observation_.Observe(
        toolbar_button_provider_->GetAvatarToolbarButton());
  }
}

AutofillBubbleHandlerImpl::~AutofillBubbleHandlerImpl() = default;

// TODO(crbug.com/1061633): Clean up this two functions and add helper for
// shared code.
AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowSaveCreditCardBubble(
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
    case BubbleType::LOCAL_CVC_SAVE:
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::UPLOAD_CVC_SAVE:
      bubble =
          new SaveCardOfferBubbleViews(anchor_view, web_contents, controller);
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

  DCHECK(icon_view);
  bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(is_user_gesture ? LocationBarBubbleDelegateView::USER_GESTURE
                               : LocationBarBubbleDelegateView::AUTOMATIC);
  return bubble;
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowIbanBubble(
    content::WebContents* web_contents,
    IbanBubbleController* controller,
    bool is_user_gesture,
    IbanBubbleType bubble_type) {
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kSaveIban);
  DCHECK(icon_view);
  views::View* anchor_view =
      toolbar_button_provider_->GetAnchorView(PageActionIconType::kSaveIban);

  // TODO(crbug.com/1416270): Add Show() to AutofillBubbleBase and refactor
  // below.
  switch (bubble_type) {
    case IbanBubbleType::kLocalSave:
    case IbanBubbleType::kUploadSave: {
      SaveIbanBubbleView* bubble =
          new SaveIbanBubbleView(anchor_view, web_contents, controller);

      DCHECK(bubble);
      bubble->SetHighlightedButton(icon_view);

      views::BubbleDialogDelegateView::CreateBubble(bubble);
      bubble->Show(is_user_gesture ? LocationBarBubbleDelegateView::USER_GESTURE
                                   : LocationBarBubbleDelegateView::AUTOMATIC);
      return bubble;
    }
    case IbanBubbleType::kManageSavedIban: {
      ManageSavedIbanBubbleView* bubble =
          new ManageSavedIbanBubbleView(anchor_view, web_contents, controller);

      DCHECK(bubble);
      bubble->SetHighlightedButton(icon_view);

      views::BubbleDialogDelegateView::CreateBubble(bubble);
      bubble->Show(is_user_gesture ? LocationBarBubbleDelegateView::USER_GESTURE
                                   : LocationBarBubbleDelegateView::AUTOMATIC);
      return bubble;
    }
    case IbanBubbleType::kInactive:
      NOTREACHED_NORETURN();
  }
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowLocalCardMigrationBubble(
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
  DCHECK(icon_view);
  bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(is_user_gesture ? LocationBarBubbleDelegateView::USER_GESTURE
                               : LocationBarBubbleDelegateView::AUTOMATIC);
  return bubble;
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowOfferNotificationBubble(
    content::WebContents* web_contents,
    OfferNotificationBubbleController* controller,
    bool is_user_gesture) {
  views::View* anchor_view = toolbar_button_provider_->GetAnchorView(
      PageActionIconType::kPaymentsOfferNotification);
  OfferNotificationBubbleViews* bubble =
      new OfferNotificationBubbleViews(anchor_view, web_contents, controller);

  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kPaymentsOfferNotification);
  DCHECK(icon_view);
  bubble->SetHighlightedButton(icon_view);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->ShowForReason(is_user_gesture
                            ? OfferNotificationBubbleViews::USER_GESTURE
                            : OfferNotificationBubbleViews::AUTOMATIC);
  return bubble;
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowSaveAddressProfileBubble(
    content::WebContents* web_contents,
    SaveUpdateAddressProfileBubbleController* controller,
    bool is_user_gesture) {
  views::View* anchor_view = toolbar_button_provider_->GetAnchorView(
      PageActionIconType::kSaveAutofillAddress);
  SaveAddressProfileView* bubble =
      new SaveAddressProfileView(anchor_view, web_contents, controller);
  DCHECK(bubble);
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kSaveAutofillAddress);
  DCHECK(icon_view);
  bubble->SetHighlightedButton(icon_view);
  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(is_user_gesture ? LocationBarBubbleDelegateView::USER_GESTURE
                               : LocationBarBubbleDelegateView::AUTOMATIC);
  return bubble;
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowUpdateAddressProfileBubble(
    content::WebContents* web_contents,
    SaveUpdateAddressProfileBubbleController* controller,
    bool is_user_gesture) {
  views::View* anchor_view = toolbar_button_provider_->GetAnchorView(
      PageActionIconType::kSaveAutofillAddress);
  UpdateAddressProfileView* bubble =
      new UpdateAddressProfileView(anchor_view, web_contents, controller);
  DCHECK(bubble);
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kSaveAutofillAddress);
  DCHECK(icon_view);
  bubble->SetHighlightedButton(icon_view);
  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->Show(is_user_gesture ? LocationBarBubbleDelegateView::USER_GESTURE
                               : LocationBarBubbleDelegateView::AUTOMATIC);
  return bubble;
}

AutofillBubbleBase*
AutofillBubbleHandlerImpl::ShowVirtualCardManualFallbackBubble(
    content::WebContents* web_contents,
    VirtualCardManualFallbackBubbleController* controller,
    bool is_user_gesture) {
  VirtualCardManualFallbackBubbleViews* bubble =
      new VirtualCardManualFallbackBubbleViews(
          toolbar_button_provider_->GetAnchorView(
              PageActionIconType::kVirtualCardManualFallback),
          web_contents, controller);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->ShowForReason(is_user_gesture
                            ? VirtualCardManualFallbackBubbleViews::USER_GESTURE
                            : VirtualCardManualFallbackBubbleViews::AUTOMATIC);
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kVirtualCardManualFallback);
  if (icon_view)
    bubble->SetHighlightedButton(icon_view);

  return bubble;
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowVirtualCardEnrollBubble(
    content::WebContents* web_contents,
    VirtualCardEnrollBubbleController* controller,
    bool is_user_gesture) {
  VirtualCardEnrollBubbleViews* bubble = new VirtualCardEnrollBubbleViews(
      toolbar_button_provider_->GetAnchorView(
          PageActionIconType::kVirtualCardEnroll),
      web_contents, controller);

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->ShowForReason(is_user_gesture
                            ? VirtualCardEnrollBubbleViews::USER_GESTURE
                            : VirtualCardEnrollBubbleViews::AUTOMATIC);
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kVirtualCardEnroll);
  if (icon_view)
    bubble->SetHighlightedButton(icon_view);

  return bubble;
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowMandatoryReauthBubble(
    content::WebContents* web_contents,
    MandatoryReauthBubbleController* controller,
    bool is_user_gesture,
    MandatoryReauthBubbleType bubble_type) {
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kMandatoryReauth);
  DCHECK(icon_view);
  views::View* anchor_view = toolbar_button_provider_->GetAnchorView(
      PageActionIconType::kMandatoryReauth);

  switch (bubble_type) {
    case MandatoryReauthBubbleType::kOptIn: {
      MandatoryReauthOptInBubbleView* bubble =
          new MandatoryReauthOptInBubbleView(anchor_view, web_contents,
                                             controller);
      bubble->SetHighlightedButton(icon_view);
      views::BubbleDialogDelegateView::CreateBubble(bubble);
      bubble->Show(is_user_gesture ? LocationBarBubbleDelegateView::USER_GESTURE
                                   : LocationBarBubbleDelegateView::AUTOMATIC);
      return bubble;
    }
    case MandatoryReauthBubbleType::kConfirmation: {
      MandatoryReauthConfirmationBubbleView* bubble =
          new MandatoryReauthConfirmationBubbleView(anchor_view, web_contents,
                                                    controller);
      bubble->SetHighlightedButton(icon_view);
      views::BubbleDialogDelegateView::CreateBubble(bubble);
      bubble->Show(is_user_gesture ? LocationBarBubbleDelegateView::USER_GESTURE
                                   : LocationBarBubbleDelegateView::AUTOMATIC);
      return bubble;
    }
    case MandatoryReauthBubbleType::kInactive:
      NOTREACHED_NORETURN();
  }
}

void AutofillBubbleHandlerImpl::OnAvatarHighlightAnimationFinished() {
  if (should_show_sign_in_promo_if_applicable_) {
    should_show_sign_in_promo_if_applicable_ = false;
    chrome::ExecuteCommand(
        browser_, IDC_SHOW_SAVE_LOCAL_CARD_SIGN_IN_PROMO_IF_APPLICABLE);
  }

  // Notify the virtual card enrollment manager that the avatar highlight
  // animation has completed in case we are offering VCN enrollment.
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;

  autofill::ContentAutofillDriverFactory* driver =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents);
  if (!driver)
    return;

  autofill::AutofillClient* autofill_client = driver->client();
  if (!autofill_client)
    return;

  raw_ptr<autofill::VirtualCardEnrollmentManager>
      virtual_card_enrollment_manager =
          autofill_client->GetVirtualCardEnrollmentManager();

  if (virtual_card_enrollment_manager)
    virtual_card_enrollment_manager->OnCardSavedAnimationComplete();
}

void AutofillBubbleHandlerImpl::ShowAvatarHighlightAnimation() {
  AvatarToolbarButton* avatar =
      toolbar_button_provider_->GetAvatarToolbarButton();
  if (avatar)
    avatar->ShowAvatarHighlightAnimation();
}

}  // namespace autofill
