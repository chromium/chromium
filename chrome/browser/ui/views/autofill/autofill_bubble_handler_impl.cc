// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_bubble_handler_impl.h"

#include <concepts>
#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/autofill/payments/save_iban_ui.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/autofill/address_sign_in_promo_view.h"
#include "chrome/browser/ui/views/autofill/autofill_ai/save_or_update_autofill_ai_data_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/filled_card_information_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/filled_card_information_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/manage_saved_iban_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_confirmation_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_opt_in_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_manage_cards_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_offer_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_iban_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_method_and_virtual_card_enroll_confirmation_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_icon_view.h"
#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"
#include "chrome/browser/ui/views/autofill/update_address_profile_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_bubble_controller.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace autofill {

namespace {

template <typename View, typename... Args>
  requires(std::derived_from<View, AutofillLocationBarBubble>)
View* ShowBubble(ToolbarButtonProvider* toolbar_button_provider,
                 std::optional<actions::ActionId> action_id,
                 PageActionIconType page_action_icon_type,
                 bool is_user_gesture,
                 Args&&... args) {
  views::View* const anchor_view =
      toolbar_button_provider->GetAnchorView(action_id);
  auto bubble =
      std::make_unique<View>(anchor_view, std::forward<Args>(args)...);
  if (!views::Button::AsButton(anchor_view)) {
    views::Button* icon_view;
    if (IsPageActionMigrated(page_action_icon_type)) {
      CHECK(action_id.has_value());
      auto* page_action =
          toolbar_button_provider->GetPageActionView(action_id.value());
      icon_view = page_action;
    } else {
      icon_view =
          toolbar_button_provider->GetPageActionIconView(page_action_icon_type);
    }
    CHECK(icon_view);
    bubble->SetHighlightedButton(icon_view);
  }

  View* const bubble_ptr = bubble.get();
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
  const LocationBarBubbleDelegateView::DisplayReason display_reason =
      is_user_gesture ? LocationBarBubbleDelegateView::USER_GESTURE
                      : LocationBarBubbleDelegateView::AUTOMATIC;
  // TODO(crbug.com/40679714): Check whether the Show methods can be eliminated
  // in favor of just using ShowForReason.
  if constexpr (requires { bubble_ptr->Show(display_reason); }) {
    bubble_ptr->Show(display_reason);
  } else {
    bubble_ptr->ShowForReason(display_reason);
  }
  return bubble_ptr;
}

template <typename View, typename... Args>
  requires std::derived_from<View, AutofillLocationBarBubble>
View* ShowAddressProfileBubble(ToolbarButtonProvider* toolbar_button_provider,
                               bool is_user_gesture,
                               Args&&... args) {
  return ShowBubble<View>(toolbar_button_provider,
                          kActionShowAddressesBubbleOrPage,
                          PageActionIconType::kAutofillAddress, is_user_gesture,
                          std::forward<Args>(args)...);
}

}  // namespace

AutofillBubbleHandlerImpl::AutofillBubbleHandlerImpl(
    ToolbarButtonProvider* toolbar_button_provider)
    : toolbar_button_provider_(toolbar_button_provider) {}

AutofillBubbleHandlerImpl::~AutofillBubbleHandlerImpl() = default;

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowSaveCreditCardBubble(
    content::WebContents* web_contents,
    SaveCardBubbleController* controller,
    bool is_user_gesture) {
  switch (controller->GetBubbleType()) {
    case BubbleType::LOCAL_SAVE:
    case BubbleType::LOCAL_CVC_SAVE:
    case BubbleType::UPLOAD_SAVE:
    case BubbleType::UPLOAD_CVC_SAVE:
    case BubbleType::UPLOAD_IN_PROGRESS:
      return ShowBubble<SaveCardOfferBubbleViews>(
          toolbar_button_provider_, kActionShowPaymentsBubbleOrPage,
          PageActionIconType::kSaveCard, is_user_gesture, web_contents,
          controller);
    case BubbleType::MANAGE_CARDS:
      return ShowBubble<SaveCardManageCardsBubbleViews>(
          toolbar_button_provider_, kActionShowPaymentsBubbleOrPage,
          PageActionIconType::kSaveCard, is_user_gesture, web_contents,
          controller);
    case BubbleType::UPLOAD_COMPLETED:
    case BubbleType::INACTIVE:
      break;
  }
  NOTREACHED();
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowIbanBubble(
    content::WebContents* web_contents,
    IbanBubbleController* controller,
    bool is_user_gesture,
    IbanBubbleType bubble_type) {
  switch (bubble_type) {
    case IbanBubbleType::kLocalSave:
    case IbanBubbleType::kUploadSave:
    case IbanBubbleType::kUploadInProgress:
      return ShowBubble<SaveIbanBubbleView>(
          toolbar_button_provider_, kActionShowPaymentsBubbleOrPage,
          PageActionIconType::kSaveIban, is_user_gesture, web_contents,
          controller);
    case IbanBubbleType::kManageSavedIban:
      return ShowBubble<ManageSavedIbanBubbleView>(
          toolbar_button_provider_, kActionShowPaymentsBubbleOrPage,
          PageActionIconType::kSaveIban, is_user_gesture, web_contents,
          controller);
    case IbanBubbleType::kUploadCompleted:
    case IbanBubbleType::kInactive:
      break;
  }
  NOTREACHED();
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowOfferNotificationBubble(
    content::WebContents* web_contents,
    OfferNotificationBubbleController* controller,
    bool is_user_gesture) {
  return ShowBubble<OfferNotificationBubbleViews>(
      toolbar_button_provider_, kActionOffersAndRewardsForPage,
      PageActionIconType::kPaymentsOfferNotification, is_user_gesture,
      web_contents, controller);
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowSaveAddressProfileBubble(
    content::WebContents* web_contents,
    std::unique_ptr<SaveAddressBubbleController> controller,
    bool is_user_gesture) {
  return ShowAddressProfileBubble<SaveAddressProfileView>(
      toolbar_button_provider_, is_user_gesture, std::move(controller),
      web_contents);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowAddressSignInPromo(
    content::WebContents* web_contents,
    const AutofillProfile& autofill_profile) {
  views::View* anchor_view =
      toolbar_button_provider_->GetAnchorView(kActionShowAddressesBubbleOrPage);
  AddressSignInPromoView* bubble =
      new AddressSignInPromoView(anchor_view, web_contents, autofill_profile);
  if (!views::Button::AsButton(anchor_view)) {
    PageActionIconView* icon_view =
        toolbar_button_provider_->GetPageActionIconView(
            PageActionIconType::kAutofillAddress);
    CHECK(icon_view);
    bubble->SetHighlightedButton(icon_view);
  }

  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->ShowForReason(LocationBarBubbleDelegateView::AUTOMATIC);
  return bubble;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowSaveAutofillAiDataBubble(
    content::WebContents* web_contents,
    autofill_ai::SaveOrUpdateAutofillAiDataController* controller) {
  return ShowBubble<autofill_ai::SaveOrUpdateAutofillAiDataBubbleView>(
      toolbar_button_provider_, kActionShowAddressesBubbleOrPage,
      PageActionIconType::kAutofillAddress, /*is_user_gesture=*/false,
      web_contents, controller);
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowUpdateAddressProfileBubble(
    content::WebContents* web_contents,
    std::unique_ptr<UpdateAddressBubbleController> controller,
    bool is_user_gesture) {
  return ShowAddressProfileBubble<UpdateAddressProfileView>(
      toolbar_button_provider_, is_user_gesture, std::move(controller),
      web_contents);
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowFilledCardInformationBubble(
    content::WebContents* web_contents,
    FilledCardInformationBubbleController* controller,
    bool is_user_gesture) {
  // TODO(crbug.com/376284059): An action ID should be created and used here
  // when this page action is migrated to the new page actions framework.
  return ShowBubble<FilledCardInformationBubbleViews>(
      toolbar_button_provider_, std::nullopt,
      PageActionIconType::kFilledCardInformation, is_user_gesture, web_contents,
      controller);
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowVirtualCardEnrollBubble(
    content::WebContents* web_contents,
    VirtualCardEnrollBubbleController* controller,
    bool is_user_gesture) {
  // TODO(crbug.com/376283926): An action ID should be created and used here
  // when Virtual Card Enroll is migrated to the new page actions framework.
  views::View* anchor_view =
      toolbar_button_provider_->GetAnchorView(std::nullopt);
  VirtualCardEnrollBubbleViews* bubble =
      new VirtualCardEnrollBubbleViews(anchor_view, web_contents, controller);

  views::BubbleDialogDelegateView::CreateBubble(bubble);

  // VirtualCardEnrollBubbleController::IsEnrollmentInProgress() == true when
  // the bubble has been accepted and the enrollment is still in progress. In
  // this case we do not want to offer enrollment again on reshow.
  if (controller->IsEnrollmentInProgress()) {
    bubble->SwitchToLoadingState();
  }

  bubble->ShowForReason(is_user_gesture
                            ? VirtualCardEnrollBubbleViews::USER_GESTURE
                            : VirtualCardEnrollBubbleViews::AUTOMATIC);
  if (!views::Button::AsButton(anchor_view)) {
    PageActionIconView* icon_view =
        toolbar_button_provider_->GetPageActionIconView(
            PageActionIconType::kVirtualCardEnroll);
    if (icon_view) {
      bubble->SetHighlightedButton(icon_view);
    }
  }

  return bubble;
}

AutofillBubbleBase*
AutofillBubbleHandlerImpl::ShowVirtualCardEnrollConfirmationBubble(
    content::WebContents* web_contents,
    VirtualCardEnrollBubbleController* controller) {
  // TODO(crbug.com/376283926): An action ID should be created and used here
  // when Virtual Card Enroll is migrated to the new page actions framework.
  views::View* anchor_view =
      toolbar_button_provider_->GetAnchorView(std::nullopt);
  base::OnceCallback<void(PaymentsUiClosedReason)> callback =
      controller->GetOnBubbleClosedCallback();
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kVirtualCardEnroll);
  const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams& ui_params =
      controller->GetConfirmationUiParams();

  return ShowSaveCardAndVirtualCardEnrollConfirmationBubble(
      anchor_view, web_contents, std::move(callback), icon_view, ui_params);
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
  // TODO(crbug.com/376283953): An action ID should be created and used here
  // when Mandatory Reauth is migrated to the new page actions framework.
  views::View* anchor_view =
      toolbar_button_provider_->GetAnchorView(std::nullopt);

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
      NOTREACHED();
  }
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowSaveCardConfirmationBubble(
    content::WebContents* web_contents,
    SaveCardBubbleController* controller) {
  views::View* anchor_view =
      toolbar_button_provider_->GetAnchorView(kActionShowPaymentsBubbleOrPage);
  base::OnceCallback<void(PaymentsUiClosedReason)> callback =
      controller->GetOnBubbleClosedCallback();
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kSaveCard);
  const SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams& ui_params =
      controller->GetConfirmationUiParams();

  return ShowSaveCardAndVirtualCardEnrollConfirmationBubble(
      anchor_view, web_contents, std::move(callback), icon_view, ui_params);
}

AutofillBubbleBase* AutofillBubbleHandlerImpl::ShowSaveIbanConfirmationBubble(
    content::WebContents* web_contents,
    IbanBubbleController* controller) {
  views::View* anchor_view =
      toolbar_button_provider_->GetAnchorView(kActionShowPaymentsBubbleOrPage);
  base::OnceCallback<void(PaymentsUiClosedReason)> callback =
      controller->GetOnBubbleClosedCallback();
  PageActionIconView* icon_view =
      toolbar_button_provider_->GetPageActionIconView(
          PageActionIconType::kSaveIban);

  return ShowSaveCardAndVirtualCardEnrollConfirmationBubble(
      anchor_view, web_contents, std::move(callback), icon_view,
      controller->GetConfirmationUiParams());
}

AutofillBubbleBase*
AutofillBubbleHandlerImpl::ShowSaveCardAndVirtualCardEnrollConfirmationBubble(
    views::View* anchor_view,
    content::WebContents* web_contents,
    base::OnceCallback<void(PaymentsUiClosedReason)> controller_hide_callback,
    PageActionIconView* icon_view,
    SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams ui_params) {
  SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews* bubble =
      new SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews(
          anchor_view, web_contents, std::move(controller_hide_callback),
          std::move(ui_params));

  if (!views::Button::AsButton(anchor_view)) {
    bubble->SetHighlightedButton(icon_view);
  }
  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->ShowForReason(LocationBarBubbleDelegateView::AUTOMATIC);

  return bubble;
}

}  // namespace autofill
