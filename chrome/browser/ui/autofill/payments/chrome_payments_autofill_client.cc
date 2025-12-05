// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_offer_manager_factory.h"
#include "chrome/browser/autofill/iban_manager_factory.h"
#include "chrome/browser/autofill/merchant_promo_code_manager_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill/payments/create_card_unmask_prompt_view.h"
#include "chrome/browser/ui/autofill/payments/credit_card_scanner_controller.h"
#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/risk_util.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_delegate.h"
#include "components/autofill/core/browser/metrics/payments/risk_data_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#include "components/autofill/core/browser/payments/otp_unmask_result.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager_impl.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/payments/autofill_error_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller_impl.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/preferences/autofill/settings_navigation_helper.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller_impl.h"
#include "chrome/browser/keyboard_accessory/android/payment_method_accessory_controller.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_controller.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view_impl.h"
#include "chrome/browser/ui/android/autofill/autofill_cvc_save_message_delegate.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_bottom_sheet_bridge.h"
#include "chrome/browser/ui/android/autofill/autofill_save_card_delegate_android.h"
#include "chrome/browser/ui/android/autofill/autofill_save_iban_bottom_sheet_bridge.h"
#include "chrome/browser/ui/android/autofill/autofill_save_iban_delegate.h"
#include "chrome/browser/ui/android/autofill/card_expiration_date_fix_flow_view_android.h"
#include "chrome/browser/ui/android/autofill/card_name_fix_flow_view_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/autofill/autofill_snackbar_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/android_bnpl_ui_delegate.h"
#include "chrome/browser/ui/autofill/payments/android_payments_window_manager.h"
#include "chrome/browser/ui/autofill/payments/autofill_message_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_message_model.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_controller_android.h"
#include "components/autofill/core/browser/payments/android_bnpl_strategy.h"
#include "components/autofill/core/browser/payments/autofill_save_iban_ui_info.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_view.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_view.h"
#include "components/webauthn/android/internal_authenticator_android.h"
#else  // !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/payments/desktop_bnpl_ui_delegate.h"
#include "chrome/browser/ui/autofill/payments/desktop_payments_window_manager.h"
#include "chrome/browser/ui/autofill/payments/filled_card_information_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_state.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck
#include "chrome/browser/ui/promos/ios_promos_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/autofill/core/browser/payments/desktop_bnpl_strategy.h"
#include "components/autofill/core/common/autofill_payments_features.h"
// TODO(crbug.com/407105162): Remove nogncheck when crbug.com/40147906 is fixed.
#include "components/tabs/public/tab_interface.h"  // nogncheck
#include "components/webauthn/content/browser/internal_authenticator_impl.h"
#endif  // BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/407106692): Refactor for Platform-Specific Code Separation.
namespace autofill::payments {

ChromePaymentsAutofillClient::ChromePaymentsAutofillClient(
    ContentAutofillClient* client)
    : content::WebContentsObserver(&client->GetWebContents()),
      client_(CHECK_DEREF(client)),
      save_and_fill_manager_(
          std::make_unique<payments::SaveAndFillManagerImpl>(&client_.get())) {}

ChromePaymentsAutofillClient::~ChromePaymentsAutofillClient() = default;

void ChromePaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  if (!risk_data_.empty() &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnablePrefetchingRiskDataForRetrieval)) {
    // Notify tests that the cached risk data was used and new risk data was not
    // loaded, if the callback exists.
    if (cached_risk_data_loaded_callback_for_testing_) {
      std::move(cached_risk_data_loaded_callback_for_testing_).Run(risk_data_);
      return;
    }
    std::move(callback).Run(risk_data_);
    return;
  }
  risk_util::LoadRiskData(
      0, web_contents(),
      base::BindOnce(&ChromePaymentsAutofillClient::OnRiskDataLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     base::TimeTicks::Now()));
}

#if BUILDFLAG(IS_ANDROID)
AutofillSaveCardBottomSheetBridge*
ChromePaymentsAutofillClient::GetOrCreateAutofillSaveCardBottomSheetBridge() {
  if (!autofill_save_card_bottom_sheet_bridge_) {
    // During shutdown the window may be null. There is no need to show the
    // bottom sheet during shutdown.
    auto* window_android = web_contents()->GetTopLevelNativeWindow();
    TabModel* tab_model =
        TabModelList::GetTabModelForWebContents(web_contents());
    if (window_android && tab_model) {
      autofill_save_card_bottom_sheet_bridge_ =
          std::make_unique<AutofillSaveCardBottomSheetBridge>(window_android,
                                                              tab_model);
    }
  }
  return autofill_save_card_bottom_sheet_bridge_.get();
}

AutofillSaveIbanBottomSheetBridge*
ChromePaymentsAutofillClient::GetOrCreateAutofillSaveIbanBottomSheetBridge() {
  if (!autofill_save_iban_bottom_sheet_bridge_) {
    // During shutdown the window may be null. There is no need to show the
    // bottom sheet during shutdown.
    auto* window_android = web_contents()->GetTopLevelNativeWindow();
    TabModel* tab_model =
        TabModelList::GetTabModelForWebContents(web_contents());
    if (window_android && tab_model) {
      autofill_save_iban_bottom_sheet_bridge_ =
          std::make_unique<AutofillSaveIbanBottomSheetBridge>(window_android,
                                                              tab_model);
    }
  }
  return autofill_save_iban_bottom_sheet_bridge_.get();
}

#else   // !BUILDFLAG(IS_ANDROID)
void ChromePaymentsAutofillClient::ShowWebauthnOfferDialog(
    WebauthnDialogCallback offer_dialog_callback) {
  WebauthnDialogControllerImpl::GetOrCreateForPage(
      web_contents()->GetPrimaryPage())
      ->ShowOfferDialog(std::move(offer_dialog_callback));
}

void ChromePaymentsAutofillClient::ShowWebauthnVerifyPendingDialog(
    WebauthnDialogCallback verify_pending_dialog_callback) {
  WebauthnDialogControllerImpl::GetOrCreateForPage(
      web_contents()->GetPrimaryPage())
      ->ShowVerifyPendingDialog(std::move(verify_pending_dialog_callback));
}

void ChromePaymentsAutofillClient::UpdateWebauthnOfferDialogWithError() {
  WebauthnDialogControllerImpl* controller =
      WebauthnDialogControllerImpl::GetForPage(
          web_contents()->GetPrimaryPage());
  if (controller) {
    controller->UpdateDialog(WebauthnDialogState::kOfferError);
  }
}

bool ChromePaymentsAutofillClient::CloseWebauthnDialog() {
  WebauthnDialogControllerImpl* controller =
      WebauthnDialogControllerImpl::GetForPage(
          web_contents()->GetPrimaryPage());
  if (controller) {
    return controller->CloseDialog();
  }

  return false;
}

void ChromePaymentsAutofillClient::
    HideVirtualCardEnrollBubbleAndIconIfVisible() {
  VirtualCardEnrollBubbleControllerImpl::CreateForWebContents(web_contents());
  VirtualCardEnrollBubbleControllerImpl* controller =
      VirtualCardEnrollBubbleControllerImpl::FromWebContents(web_contents());

  if (controller && controller->IsIconVisible()) {
    controller->HideIconAndBubble();
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
void ChromePaymentsAutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const std::u16string&)> callback) {
  CardNameFixFlowViewAndroid* card_name_fix_flow_view_android =
      new CardNameFixFlowViewAndroid(&card_name_fix_flow_controller_,
                                     web_contents());
  card_name_fix_flow_controller_.Show(
      card_name_fix_flow_view_android, GetAccountHolderName(),
      /*name_accepted_callback=*/std::move(callback));
}

void ChromePaymentsAutofillClient::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
        callback) {
  CardExpirationDateFixFlowViewAndroid*
      card_expiration_date_fix_flow_view_android =
          new CardExpirationDateFixFlowViewAndroid(
              &card_expiration_date_fix_flow_controller_, web_contents());
  card_expiration_date_fix_flow_controller_.Show(
      card_expiration_date_fix_flow_view_android, card,
      /*callback=*/std::move(callback));
}
#endif  // BUILDFLAG(IS_ANDROID)

bool ChromePaymentsAutofillClient::HasCreditCardScanFeature() const {
  return CreditCardScannerController::HasCreditCardScanFeature();
}

void ChromePaymentsAutofillClient::ScanCreditCard(
    ChromePaymentsAutofillClient::CreditCardScanCallback callback) {
  CreditCardScannerController::ScanCreditCard(web_contents(),
                                              std::move(callback));
}

bool ChromePaymentsAutofillClient::LocalCardSaveIsSupported() {
  return true;
}

void ChromePaymentsAutofillClient::ShowSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  DCHECK(options.show_prompt);
  AutofillSaveCardUiInfo ui_info =
      AutofillSaveCardUiInfo::CreateForLocalSave(options, card);
  auto save_card_delegate = std::make_unique<AutofillSaveCardDelegateAndroid>(
      std::move(callback), options, web_contents());

  // If a CVC is detected for an existing local card in the checkout form, the
  // CVC save prompt is shown in a message.
  if (options.card_save_type == CardSaveType::kCvcSaveOnly) {
    autofill_cvc_save_message_delegate_ =
        std::make_unique<AutofillCvcSaveMessageDelegate>(web_contents());
    autofill_cvc_save_message_delegate_->ShowMessage(
        ui_info, std::move(save_card_delegate));
    return;
  }

  // Saving a new local card (may include CVC) via a bottom sheet.
  if (auto* bridge = GetOrCreateAutofillSaveCardBottomSheetBridge()) {
    bridge->RequestShowContent(ui_info, std::move(save_card_delegate));
  }
#else   // !BUILDFLAG(IS_ANDROID)
  SaveCardBubbleControllerImpl::CreateForWebContents(web_contents());
  SaveCardBubbleControllerImpl::FromWebContents(web_contents())
      ->OfferLocalSave(card, options, std::move(callback));
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromePaymentsAutofillClient::ShowSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  DCHECK(options.show_prompt);
  Profile* profile =
      !web_contents()
          ? nullptr
          : Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  AutofillSaveCardUiInfo ui_info = AutofillSaveCardUiInfo::CreateForUploadSave(
      options, card, legal_message_lines, account_info);
  auto save_card_delegate = std::make_unique<AutofillSaveCardDelegateAndroid>(
      std::move(callback), options, web_contents());

  // If a CVC is detected for an existing server card in the checkout form,
  // the CVC save prompt is shown in a message.
  if (options.card_save_type == CardSaveType::kCvcSaveOnly) {
    autofill_cvc_save_message_delegate_ =
        std::make_unique<AutofillCvcSaveMessageDelegate>(web_contents());
    autofill_cvc_save_message_delegate_->ShowMessage(
        ui_info, std::move(save_card_delegate));
    return;
  }

  // For new cards, the save card prompt is shown in a bottom sheet.
  if (auto* bridge = GetOrCreateAutofillSaveCardBottomSheetBridge()) {
    bridge->RequestShowContent(ui_info, std::move(save_card_delegate));
  }
#else
  // Hide virtual card confirmation bubble showing for a different card.
  HideVirtualCardEnrollBubbleAndIconIfVisible();

  // Do lazy initialization of SaveCardBubbleControllerImpl.
  SaveCardBubbleControllerImpl::CreateForWebContents(web_contents());
  SaveCardBubbleControllerImpl::FromWebContents(web_contents())
      ->OfferUploadSave(card, legal_message_lines, options,
                        std::move(callback));
#endif
}

void ChromePaymentsAutofillClient::CreditCardUploadCompleted(
    PaymentsRpcResult result,
    std::optional<OnConfirmationClosedCallback>
        on_confirmation_closed_callback) {
  const bool card_saved = result == PaymentsRpcResult::kSuccess;
#if BUILDFLAG(IS_ANDROID)
  if (auto* bridge = GetOrCreateAutofillSaveCardBottomSheetBridge()) {
    bridge->Hide();
  }

  if (card_saved) {
    if (on_confirmation_closed_callback) {
      client_->GetAutofillSnackbarController()->ShowWithDurationAndCallback(
          AutofillSnackbarType::kSaveCardSuccess,
          kSaveCardConfirmationSnackbarDuration, base::DoNothing(),
          std::move(on_confirmation_closed_callback));
    } else {
      client_->GetAutofillSnackbarController()->Show(
          AutofillSnackbarType::kSaveCardSuccess, base::DoNothing());
    }
  } else if (result != PaymentsRpcResult::kClientSideTimeout) {
    GetAutofillMessageController().Show(
        AutofillMessageModel::CreateForSaveCardFailure());
  }
#else  // !BUILDFLAG(IS_ANDROID)
  if (result == PaymentsRpcResult::kClientSideTimeout) {
    HideSaveCardPrompt();
    return;
  }
  // This feedback is also used by Save and Fill flow where
  // SaveCardBubbleControllerImpl is not created beforehand.
  if (SaveCardBubbleControllerImpl* controller =
          SaveCardBubbleControllerImpl::GetOrCreateForWebContents(
              web_contents())) {
    // Only attempt to show the iOS payment promo if the card was successfully
    // uploaded and there is no VCN enroll flow callback, and still fallback to
    // normal confirmation bubble if showing the promo fails.
    if (card_saved && !on_confirmation_closed_callback) {
      base::OnceClosure promo_will_show_callback =
          controller->GetEndSaveCardPromptFlowCallback();
      base::OnceClosure promo_not_shown_callback =
          controller->GetShowConfirmationForCardSuccessfullySavedCallback();

      Browser* browser = chrome::FindBrowserWithTab(web_contents());

      if (!browser) {
        std::move(promo_not_shown_callback).Run();
        return;
      }

      ios_promos_utils::MaybeOverrideCardConfirmationBubbleWithIOSPaymentPromo(
          browser, std::move(promo_will_show_callback),
          std::move(promo_not_shown_callback));

      return;
    }

    controller->ShowConfirmationBubbleView(
        card_saved, std::move(on_confirmation_closed_callback));
  }
#endif
}

void ChromePaymentsAutofillClient::HideSaveCardPrompt() {
#if !BUILDFLAG(IS_ANDROID)
  SaveCardBubbleControllerImpl* controller =
      SaveCardBubbleControllerImpl::FromWebContents(web_contents());
  if (controller) {
    controller->HideSaveCardBubble();
  }
#endif
}

void ChromePaymentsAutofillClient::ShowVirtualCardEnrollDialog(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {
  VirtualCardEnrollBubbleControllerImpl::CreateForWebContents(web_contents());
  VirtualCardEnrollBubbleControllerImpl* controller =
      VirtualCardEnrollBubbleControllerImpl::FromWebContents(web_contents());
  DCHECK(controller);
  controller->SetupAndShowBubble(virtual_card_enrollment_fields,
                                 std::move(accept_virtual_card_callback),
                                 std::move(decline_virtual_card_callback));
}

void ChromePaymentsAutofillClient::VirtualCardEnrollCompleted(
    PaymentsRpcResult result) {
  VirtualCardEnrollBubbleControllerImpl::CreateForWebContents(web_contents());
  VirtualCardEnrollBubbleControllerImpl* controller =
      VirtualCardEnrollBubbleControllerImpl::FromWebContents(web_contents());

  if (controller) {
    // Called by clank to close AutofillVCNEnrollBottomSheetBridge.
    // TODO(crbug.com/350713949): Extract AutofillVCNEnrollBottomSheetBridge
    // so the controller only needs to be called for desktop.
    controller->ShowConfirmationBubbleView(result);
  }

#if BUILDFLAG(IS_ANDROID)
  if (result == PaymentsRpcResult::kSuccess) {
    client_->GetAutofillSnackbarController()->Show(
        AutofillSnackbarType::kVirtualCardEnrollSuccess, base::DoNothing());
  } else if (controller && result != PaymentsRpcResult::kClientSideTimeout) {
    GetAutofillMessageController().Show(
        AutofillMessageModel::CreateForVirtualCardEnrollFailure(
            /*card_label=*/controller->GetUiModel()
                .enrollment_fields()
                .credit_card.NetworkAndLastFourDigits()));
  }
#endif
}

void ChromePaymentsAutofillClient::OnCardDataAvailable(
    const FilledCardInformationBubbleOptions& options) {
#if BUILDFLAG(IS_ANDROID)
  // Note that currently the snackbar is displayed only for virtual cards or
  // cards enrolled in card info retrieval. In the case for BNPL, it is a
  // one-time use virtual card.
  AutofillSnackbarType type;
  if (options.filled_card.is_bnpl_card()) {
    type = AutofillSnackbarType::kBnpl;
  } else {
    type = options.filled_card.record_type() ==
                   CreditCard::RecordType::kVirtualCard
               ? AutofillSnackbarType::kVirtualCard
               : AutofillSnackbarType::kCardInfoRetrieval;
  }

  // Credit card manual filling sheet should always be created on the Java side.
  // This happens after the first time `PaymentMethodAccessoryControllerImpl`
  // pushes data to the Java keyboard accessory. Before that, credit card manual
  // filling sheet doesn't exist on the java side. The data is pushed
  // asynchronously in `ManualFillingViewAndroid::OnItemsAvailable`. Manually
  // refresh credit card suggestions before the snackbar is shown so that the
  // credit card manual filling sheet can be opened from the snackbar.
  // TODO(crbug.com/430575808): Consider adding a synchronous version of the
  // `RefreshSuggestions` so that the race condition is removed completely.
  PaymentMethodAccessoryController::GetOrCreate(web_contents())
      ->RefreshSuggestions();

  client_->GetAutofillSnackbarController()->ShowPaymentsSnackbar(
      type, options.filled_card,
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> contents) {
            if (!contents) {
              return;
            }
            ManualFillingControllerImpl::GetOrCreate(contents.get())
                ->ShowAccessorySheetTab(
                    autofill::AccessoryTabType::CREDIT_CARDS);
          },
          web_contents()->GetWeakPtr()));
#else
  FilledCardInformationBubbleControllerImpl::CreateForWebContents(
      web_contents());
  FilledCardInformationBubbleControllerImpl* controller =
      FilledCardInformationBubbleControllerImpl::FromWebContents(
          web_contents());
  controller->SetupAndShowBubble(options);
#endif
}

void ChromePaymentsAutofillClient::ConfirmSaveIbanLocally(
    const Iban& iban,
    bool should_show_prompt,
    SaveIbanPromptCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  // For new IBANs, the save IBAN prompt is shown in a bottom sheet.
  if (auto* bridge = GetOrCreateAutofillSaveIbanBottomSheetBridge()) {
    auto save_iban_delegate = std::make_unique<AutofillSaveIbanDelegate>(
        std::move(callback), web_contents());
    AutofillSaveIbanUiInfo ui_info = AutofillSaveIbanUiInfo::CreateForLocalSave(
        iban.GetIdentifierStringForAutofillDisplay(
            /*is_value_masked=*/false));
    bridge->RequestShowContent(ui_info, std::move(save_iban_delegate));
  }
#else
  // Do lazy initialization of IbanBubbleControllerImpl.
  IbanBubbleControllerImpl::CreateForWebContents(web_contents());
  IbanBubbleControllerImpl::FromWebContents(web_contents())
      ->OfferLocalSave(iban, should_show_prompt, std::move(callback));
#endif
}

void ChromePaymentsAutofillClient::ConfirmUploadIbanToCloud(
    const Iban& iban,
    LegalMessageLines legal_message_lines,
    bool should_show_prompt,
    SaveIbanPromptCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  AutofillSaveIbanUiInfo ui_info = AutofillSaveIbanUiInfo::CreateForUploadSave(
      iban.GetIdentifierStringForAutofillDisplay(
          /*is_value_masked=*/false),
      legal_message_lines);

  // Upload a new IBAN to the server via a Bottom Sheet.
  if (auto* bridge = GetOrCreateAutofillSaveIbanBottomSheetBridge()) {
    bridge->RequestShowContent(
        ui_info, std::make_unique<AutofillSaveIbanDelegate>(std::move(callback),
                                                            web_contents()));
  }
#else
  // Do lazy initialization of IbanBubbleControllerImpl.
  IbanBubbleControllerImpl::CreateForWebContents(web_contents());
  IbanBubbleControllerImpl::FromWebContents(web_contents())
      ->OfferUploadSave(iban, std::move(legal_message_lines),
                        should_show_prompt, std::move(callback));
#endif
}

void ChromePaymentsAutofillClient::IbanUploadCompleted(bool iban_saved,
                                                       bool hit_max_strikes) {
#if BUILDFLAG(IS_ANDROID)
  if (auto* bridge = GetOrCreateAutofillSaveIbanBottomSheetBridge()) {
    bridge->Hide();
  }
  if (iban_saved) {
    client_->GetAutofillSnackbarController()->Show(
        AutofillSnackbarType::kSaveServerIbanSuccess,
        /*on_action_clicked_callback=*/base::DoNothing());
  }
#else  // BUILDFLAG(IS_ANDROID)
  if (IbanBubbleControllerImpl* controller =
          IbanBubbleControllerImpl::FromWebContents(web_contents())) {
    controller->ShowConfirmationBubbleView(iban_saved, hit_max_strikes);
  }
#endif
}

void ChromePaymentsAutofillClient::ShowAutofillProgressDialog(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  autofill_progress_dialog_controller_ =
      std::make_unique<AutofillProgressDialogControllerImpl>(
          autofill_progress_dialog_type, std::move(cancel_callback));
  autofill_progress_dialog_controller_->ShowDialog(
      base::BindOnce(&CreateAndShowProgressDialog,
                     autofill_progress_dialog_controller_->GetWeakPtr(),
                     base::Unretained(web_contents())));
}

void ChromePaymentsAutofillClient::CloseAutofillProgressDialog(
    bool show_confirmation_before_closing,
    base::OnceClosure no_interactive_authentication_callback) {
  DCHECK(autofill_progress_dialog_controller_);
  autofill_progress_dialog_controller_->DismissDialog(
      show_confirmation_before_closing,
      std::move(no_interactive_authentication_callback));
}

void ChromePaymentsAutofillClient::ShowCardUnmaskOtpInputDialog(
    CreditCard::RecordType card_type,
    const CardUnmaskChallengeOption& challenge_option,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {
  card_unmask_otp_input_dialog_controller_ =
      std::make_unique<CardUnmaskOtpInputDialogControllerImpl>(
          card_type, challenge_option, delegate);
  card_unmask_otp_input_dialog_controller_->ShowDialog(
      base::BindOnce(&CreateAndShowOtpInputDialog,
                     card_unmask_otp_input_dialog_controller_->GetWeakPtr(),
                     base::Unretained(web_contents())));
}

void ChromePaymentsAutofillClient::OnUnmaskOtpVerificationResult(
    OtpUnmaskResult unmask_result) {
  if (card_unmask_otp_input_dialog_controller_) {
    card_unmask_otp_input_dialog_controller_->OnOtpVerificationResult(
        unmask_result);
  }
}

void ChromePaymentsAutofillClient::ShowUnmaskAuthenticatorSelectionDialog(
    const std::vector<CardUnmaskChallengeOption>& challenge_options,
    base::OnceCallback<void(const std::string&)>
        confirm_unmask_challenge_option_callback,
    base::OnceClosure cancel_unmasking_closure) {
  card_unmask_authentication_selection_controller_ =
      std::make_unique<CardUnmaskAuthenticationSelectionDialogControllerImpl>(
          challenge_options,
          std::move(confirm_unmask_challenge_option_callback),
          std::move(cancel_unmasking_closure));
  card_unmask_authentication_selection_controller_->ShowDialog(
      base::BindOnce(&CreateAndShowCardUnmaskAuthenticationSelectionDialog,
                     base::Unretained(web_contents())));
}

void ChromePaymentsAutofillClient::DismissUnmaskAuthenticatorSelectionDialog(
    bool server_success) {
  if (card_unmask_authentication_selection_controller_) {
    card_unmask_authentication_selection_controller_
        ->DismissDialogUponServerProcessedAuthenticationMethodRequest(
            server_success);
    card_unmask_authentication_selection_controller_.reset();
  }
}

PaymentsNetworkInterface*
ChromePaymentsAutofillClient::GetPaymentsNetworkInterface() {
  if (!payments_network_interface_) {
    payments_network_interface_ = std::make_unique<PaymentsNetworkInterface>(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->GetURLLoaderFactory(),
        client_->GetIdentityManager(),
        &client_->GetPersonalDataManager().payments_data_manager(),
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->IsOffTheRecord());
  }
  return payments_network_interface_.get();
}

MultipleRequestPaymentsNetworkInterface*
ChromePaymentsAutofillClient::GetMultipleRequestPaymentsNetworkInterface() {
  if (!multiple_request_payments_network_interface_) {
    multiple_request_payments_network_interface_ =
        std::make_unique<MultipleRequestPaymentsNetworkInterface>(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext())
                ->GetURLLoaderFactory(),
            *client_->GetIdentityManager(),
            Profile::FromBrowserContext(web_contents()->GetBrowserContext())
                ->IsOffTheRecord());
  }
  return multiple_request_payments_network_interface_.get();
}

void ChromePaymentsAutofillClient::ShowAutofillErrorDialog(
    AutofillErrorDialogContext context) {
  autofill_error_dialog_controller_ =
      std::make_unique<AutofillErrorDialogControllerImpl>(std::move(context));
  autofill_error_dialog_controller_->Show(
      base::BindOnce(&CreateAndShowAutofillErrorDialog,
                     base::Unretained(autofill_error_dialog_controller_.get()),
                     base::Unretained(web_contents())));
}

PaymentsWindowManager*
ChromePaymentsAutofillClient::GetPaymentsWindowManager() {
  if (!payments_window_manager_) {
#if BUILDFLAG(IS_ANDROID)
    payments_window_manager_ =
        std::make_unique<AndroidPaymentsWindowManager>(&client_.get());
#else
    payments_window_manager_ =
        std::make_unique<DesktopPaymentsWindowManager>(&client_.get());
#endif  // BUILDFLAG(IS_ANDROID)
  }

  return payments_window_manager_.get();
}

void ChromePaymentsAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_ = std::make_unique<CardUnmaskPromptControllerImpl>(
      user_prefs::UserPrefs::Get(client_->GetWebContents().GetBrowserContext()),
      card, card_unmask_prompt_options, delegate);
  unmask_controller_->ShowPrompt(base::BindOnce(
      &CreateCardUnmaskPromptView, base::Unretained(unmask_controller_.get()),
      base::Unretained(web_contents())));
}


// TODO(crbug.com/40186650): Refactor this for both CVC and Biometrics flows.
void ChromePaymentsAutofillClient::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  if (unmask_controller_) {
    unmask_controller_->OnVerificationResult(result);
  }
#if BUILDFLAG(IS_ANDROID)
  // For VCN-related errors, on Android we show a new error dialog instead of
  // updating the CVC unmask prompt with the error message.
  switch (result) {
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      ShowAutofillErrorDialog(
          AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
              /*is_permanent_error=*/true));
      break;
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
      ShowAutofillErrorDialog(
          AutofillErrorDialogContext::WithVirtualCardPermanentOrTemporaryError(
              /*is_permanent_error=*/false));
      break;
    case PaymentsRpcResult::kSuccess:
    case PaymentsRpcResult::kTryAgainFailure:
    case PaymentsRpcResult::kPermanentFailure:
    case PaymentsRpcResult::kNetworkError:
    case PaymentsRpcResult::kClientSideTimeout:
      // Do nothing
      break;
    case PaymentsRpcResult::kNone:
      NOTREACHED();
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

VirtualCardEnrollmentManager*
ChromePaymentsAutofillClient::GetVirtualCardEnrollmentManager() {
  if (!virtual_card_enrollment_manager_) {
    PaymentsNetworkInterfaceVariation payments_network_interface;
    if (base::FeatureList::IsEnabled(
            features::
                kAutofillEnableMultipleRequestInVirtualCardDownstreamEnrollment)) {
      payments_network_interface = GetMultipleRequestPaymentsNetworkInterface();
    } else {
      payments_network_interface = GetPaymentsNetworkInterface();
    }
    virtual_card_enrollment_manager_ =
        std::make_unique<VirtualCardEnrollmentManager>(
            &client_->GetPersonalDataManager().payments_data_manager(),
            payments_network_interface, &client_.get());
  }

  return virtual_card_enrollment_manager_.get();
}

CreditCardCvcAuthenticator&
ChromePaymentsAutofillClient::GetCvcAuthenticator() {
  if (!cvc_authenticator_) {
    cvc_authenticator_ =
        std::make_unique<CreditCardCvcAuthenticator>(&client_.get());
  }
  return *cvc_authenticator_;
}

CreditCardOtpAuthenticator*
ChromePaymentsAutofillClient::GetOtpAuthenticator() {
  if (!otp_authenticator_) {
    otp_authenticator_ =
        std::make_unique<CreditCardOtpAuthenticator>(&client_.get());
  }
  return otp_authenticator_.get();
}

CreditCardRiskBasedAuthenticator*
ChromePaymentsAutofillClient::GetRiskBasedAuthenticator() {
  if (!risk_based_authenticator_) {
    risk_based_authenticator_ =
        std::make_unique<CreditCardRiskBasedAuthenticator>(&client_.get());
  }
  return risk_based_authenticator_.get();
}

bool ChromePaymentsAutofillClient::IsRiskBasedAuthEffectivelyAvailable() const {
  return true;
}

bool ChromePaymentsAutofillClient::IsMandatoryReauthEnabled() {
  return GetPaymentsDataManager().IsPaymentMethodsMandatoryReauthEnabled();
}

void ChromePaymentsAutofillClient::ShowMandatoryReauthOptInPrompt(
    base::OnceClosure accept_mandatory_reauth_callback,
    base::OnceClosure cancel_mandatory_reauth_callback,
    base::RepeatingClosure close_mandatory_reauth_callback) {
  MandatoryReauthBubbleControllerImpl::CreateForWebContents(web_contents());
  MandatoryReauthBubbleControllerImpl::FromWebContents(web_contents())
      ->SetupAndShowBubble(std::move(accept_mandatory_reauth_callback),
                           std::move(cancel_mandatory_reauth_callback),
                           std::move(close_mandatory_reauth_callback));
}

void ChromePaymentsAutofillClient::ShowMandatoryReauthOptInConfirmation() {
#if BUILDFLAG(IS_ANDROID)
  client_->GetAutofillSnackbarController()->Show(
      AutofillSnackbarType::kMandatoryReauth,
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> contents) {
            if (contents) {
              return;
            }
            // For mandatory reauth snackbar, we will show Android credit card
            // settings page.
            ShowAutofillCreditCardSettings(contents.get());
          },
          web_contents()->GetWeakPtr()));
#else
  MandatoryReauthBubbleControllerImpl::CreateForWebContents(web_contents());
  // TODO(crbug.com/4555994): Pass in the bubble type as a parameter so we
  // enforce that the confirmation bubble is shown.
  MandatoryReauthBubbleControllerImpl::FromWebContents(web_contents())
      ->ReshowBubble();
#endif
}

bool ChromePaymentsAutofillClient::IsAutofillPaymentMethodsEnabled() const {
  return autofill_payment_methods_supported_ &&
         prefs::IsAutofillPaymentMethodsEnabled(
             Profile::FromBrowserContext(web_contents()->GetBrowserContext())
                 ->GetPrefs());
}

void ChromePaymentsAutofillClient::DisablePaymentsAutofill() {
  autofill_payment_methods_supported_ = false;
}

IbanManager* ChromePaymentsAutofillClient::GetIbanManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return IbanManagerFactory::GetForProfile(profile);
}

IbanAccessManager* ChromePaymentsAutofillClient::GetIbanAccessManager() {
  if (!iban_access_manager_) {
    iban_access_manager_ = std::make_unique<IbanAccessManager>(&client_.get());
  }
  return iban_access_manager_.get();
}

MerchantPromoCodeManager*
ChromePaymentsAutofillClient::GetMerchantPromoCodeManager() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return MerchantPromoCodeManagerFactory::GetForProfile(profile);
}

void ChromePaymentsAutofillClient::OpenPromoCodeOfferDetailsURL(
    const GURL& url) {
  web_contents()->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
                             /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
}

AutofillOfferManager* ChromePaymentsAutofillClient::GetAutofillOfferManager() {
  return AutofillOfferManagerFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

void ChromePaymentsAutofillClient::UpdateOfferNotification(
    const AutofillOfferData& offer,
    const OfferNotificationOptions& options) {
  const CreditCard* card = offer.GetEligibleInstrumentIds().empty()
                               ? nullptr
                               : client_->GetPersonalDataManager()
                                     .payments_data_manager()
                                     .GetCreditCardByInstrumentId(
                                         offer.GetEligibleInstrumentIds()[0]);

  if (offer.IsCardLinkedOffer() && !card) {
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  if (options.notification_has_been_shown) {
    // For Android, if notification has been shown on this merchant, don't show
    // it again.
    return;
  }
  OfferNotificationControllerAndroid::CreateForWebContents(web_contents());
  OfferNotificationControllerAndroid* controller =
      OfferNotificationControllerAndroid::FromWebContents(web_contents());
  controller->ShowIfNecessary(&offer, card);
#else
  OfferNotificationBubbleControllerImpl::CreateForWebContents(web_contents());
  OfferNotificationBubbleControllerImpl* controller =
      OfferNotificationBubbleControllerImpl::FromWebContents(web_contents());
  controller->ShowOfferNotificationIfApplicable(offer, card, options);
#endif
}

void ChromePaymentsAutofillClient::DismissOfferNotification() {
#if BUILDFLAG(IS_ANDROID)
  OfferNotificationControllerAndroid::CreateForWebContents(web_contents());
  OfferNotificationControllerAndroid* controller =
      OfferNotificationControllerAndroid::FromWebContents(web_contents());
  controller->Dismiss();
#else
  if (auto* controller = OfferNotificationBubbleControllerImpl::FromWebContents(
          web_contents())) {
    controller->DismissNotification();
  }
#endif
}

bool ChromePaymentsAutofillClient::ShowTouchToFillCreditCard(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const Suggestion> suggestions) {
#if BUILDFLAG(IS_ANDROID)
  // Create the manual filling controller which will be used to show the
  // unmasked virtual card details in the manual fallback.
  ManualFillingController::GetOrCreate(web_contents())
      ->UpdateSourceAvailability(
          ManualFillingController::FillingSource::CREDIT_CARD_FALLBACKS,
          !suggestions.empty());

  return GetTouchToFillPaymentMethodController()->ShowPaymentMethods(
      std::make_unique<TouchToFillPaymentMethodViewImpl>(web_contents()),
      delegate, std::move(suggestions));
#else
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
#endif
}

bool ChromePaymentsAutofillClient::ShowTouchToFillIban(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const autofill::Iban> ibans_to_suggest) {
#if BUILDFLAG(IS_ANDROID)
  return GetTouchToFillPaymentMethodController()->ShowIbans(
      std::make_unique<TouchToFillPaymentMethodViewImpl>(web_contents()),
      delegate, std::move(ibans_to_suggest));
#else
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
#endif
}

bool ChromePaymentsAutofillClient::ShowTouchToFillLoyaltyCard(
    base::WeakPtr<TouchToFillDelegate> delegate,
    std::vector<autofill::LoyaltyCard> loyalty_cards_to_suggest) {
#if BUILDFLAG(IS_ANDROID)
  const GURL& current_domain = client_->GetLastCommittedPrimaryMainFrameURL();

  std::vector<autofill::LoyaltyCard> affiliated_loyalty_cards;
  std::ranges::copy_if(loyalty_cards_to_suggest,
                       std::back_inserter(affiliated_loyalty_cards),
                       [&current_domain](const autofill::LoyaltyCard& card) {
                         return card.GetAffiliationCategory(current_domain) ==
                                LoyaltyCard::AffiliationCategory::kAffiliated;
                       });

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  const bool first_time_usage =
      tracker && tracker->IsInitialized() &&
      tracker->WouldTriggerHelpUI(
          feature_engagement::kIPHAutofillEnableLoyaltyCardsFeature);

  const bool loyalty_cards_shown =
      GetTouchToFillPaymentMethodController()->ShowLoyaltyCards(
          std::make_unique<TouchToFillPaymentMethodViewImpl>(web_contents()),
          delegate, std::move(affiliated_loyalty_cards),
          std::move(loyalty_cards_to_suggest), first_time_usage);
  if (first_time_usage && loyalty_cards_shown) {
    tracker->NotifyEvent("keyboard_accessory_loyalty_cards_autofilled");
  }
  return loyalty_cards_shown;
#else
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
#endif
}

bool ChromePaymentsAutofillClient::OnPurchaseAmountExtracted(
    base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
    std::optional<int64_t> extracted_amount,
    bool is_amount_supported_by_any_issuer,
    const std::optional<std::string>& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
#if BUILDFLAG(IS_ANDROID)
  return GetTouchToFillPaymentMethodController()->OnPurchaseAmountExtracted(
      bnpl_issuer_contexts, extracted_amount, is_amount_supported_by_any_issuer,
      app_locale, std::move(selected_issuer_callback),
      std::move(cancel_callback));
#else
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
#endif
}

bool ChromePaymentsAutofillClient::ShowTouchToFillProgress(
    base::OnceClosure cancel_callback) {
#if BUILDFLAG(IS_ANDROID)
  // TTF should already be shown, so pass nullptr for `view`.
  return GetTouchToFillPaymentMethodController()->ShowProgressScreen(
      /*view=*/nullptr, std::move(cancel_callback));
#else
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
#endif
}

bool ChromePaymentsAutofillClient::ShowTouchToFillBnplIssuers(
    base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
    const std::string& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
#if BUILDFLAG(IS_ANDROID)
  return GetTouchToFillPaymentMethodController()->ShowBnplIssuers(
      bnpl_issuer_contexts, app_locale, std::move(selected_issuer_callback),
      std::move(cancel_callback));
#else
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
#endif
}

bool ChromePaymentsAutofillClient::ShowTouchToFillBnplTos(
    BnplTosModel bnpl_tos_model,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback) {
#if BUILDFLAG(IS_ANDROID)
  return GetTouchToFillPaymentMethodController()->ShowBnplIssuerTos(
      std::move(bnpl_tos_model), std::move(accept_callback),
      std::move(cancel_callback));
#else
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
#endif
}

bool ChromePaymentsAutofillClient::ShowTouchToFillError(
    const AutofillErrorDialogContext& context) {
#if BUILDFLAG(IS_ANDROID)
  // Use temporary `AutofillErrorDialogControllerImpl` to get error title and
  // description strings.
  AutofillErrorDialogControllerImpl autofill_error_dialog_controller =
      AutofillErrorDialogControllerImpl(std::move(context));

  // TTF should already be shown, so pass nullptr for `view`.
  return GetTouchToFillPaymentMethodController()->ShowErrorScreen(
      /*view=*/nullptr, autofill_error_dialog_controller.GetTitle(),
      autofill_error_dialog_controller.GetDescription());
#else
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
#endif
}

void ChromePaymentsAutofillClient::HideTouchToFillPaymentMethod() {
#if BUILDFLAG(IS_ANDROID)
  GetTouchToFillPaymentMethodController()->Hide();
#else
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
#endif
}

void ChromePaymentsAutofillClient::SetTouchToFillVisible(bool visible) {
#if BUILDFLAG(IS_ANDROID)
  GetTouchToFillPaymentMethodController()->SetVisible(visible);
#else
  // Touch To Fill is not supported on Desktop.
  NOTREACHED();
#endif
}

PaymentsDataManager& ChromePaymentsAutofillClient::GetPaymentsDataManager() {
  return client_->GetPersonalDataManager().payments_data_manager();
}

std::unique_ptr<webauthn::InternalAuthenticator>
ChromePaymentsAutofillClient::CreateCreditCardInternalAuthenticator(
    AutofillDriver* driver) {
  auto* cad = static_cast<ContentAutofillDriver*>(driver);
  content::RenderFrameHost* rfh = cad->render_frame_host();
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<webauthn::InternalAuthenticatorAndroid>(rfh);
#else
  return std::make_unique<content::InternalAuthenticatorImpl>(rfh);
#endif
}

payments::MandatoryReauthManager*
ChromePaymentsAutofillClient::GetOrCreatePaymentsMandatoryReauthManager() {
  if (!payments_mandatory_reauth_manager_) {
    payments_mandatory_reauth_manager_ =
        std::make_unique<payments::MandatoryReauthManager>(&client_.get());
  }

  return payments_mandatory_reauth_manager_.get();
}

SaveAndFillManager* ChromePaymentsAutofillClient::GetSaveAndFillManager() {
#if BUILDFLAG(IS_ANDROID)
  return nullptr;
#else
  return save_and_fill_manager_.get();
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromePaymentsAutofillClient::ShowCreditCardLocalSaveAndFillDialog(
    CardSaveAndFillDialogCallback callback) {
#if !BUILDFLAG(IS_ANDROID)
  if (!save_and_fill_dialog_controller_) {
    save_and_fill_dialog_controller_ =
        std::make_unique<SaveAndFillDialogControllerImpl>();
  }
  save_and_fill_dialog_controller_->ShowLocalDialog(
      base::BindOnce(&CreateAndShowSaveAndFillDialog,
                     save_and_fill_dialog_controller_->GetWeakPtr(),
                     web_contents()),
      std::move(callback));
#else
  NOTIMPLEMENTED();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromePaymentsAutofillClient::ShowCreditCardUploadSaveAndFillDialog(
    const LegalMessageLines& legal_message_lines,
    CardSaveAndFillDialogCallback callback) {
#if !BUILDFLAG(IS_ANDROID)
  CHECK(save_and_fill_dialog_controller_);
  save_and_fill_dialog_controller_->ShowUploadDialog(
      std::move(legal_message_lines),
      base::BindOnce(&CreateAndShowSaveAndFillDialog,
                     save_and_fill_dialog_controller_->GetWeakPtr(),
                     web_contents()),
      std::move(callback));
#else
  NOTIMPLEMENTED();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromePaymentsAutofillClient::ShowCreditCardSaveAndFillPendingDialog() {
#if !BUILDFLAG(IS_ANDROID)
  if (!save_and_fill_dialog_controller_) {
    save_and_fill_dialog_controller_ =
        std::make_unique<SaveAndFillDialogControllerImpl>();
  }
  save_and_fill_dialog_controller_->ShowPendingDialog(base::BindOnce(
      &CreateAndShowSaveAndFillDialog,
      save_and_fill_dialog_controller_->GetWeakPtr(), web_contents()));
#else
  NOTIMPLEMENTED();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromePaymentsAutofillClient::HideCreditCardSaveAndFillDialog() {
#if !BUILDFLAG(IS_ANDROID)
  if (save_and_fill_dialog_controller_) {
    save_and_fill_dialog_controller_->Dismiss();
  }
#else
  NOTIMPLEMENTED();
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool ChromePaymentsAutofillClient::IsTabModalPopupDeprecated() const {
#if !BUILDFLAG(IS_ANDROID)
  tabs::TabInterface* const tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  return tab_interface && tab_interface->GetBrowserWindowInterface()
                              ->IsTabModalPopupDeprecated();
#else
  return false;
#endif  // !BUILDFLAG(IS_ANDROID)
}

BnplStrategy* ChromePaymentsAutofillClient::GetBnplStrategy() {
  if (!bnpl_strategy_) {
#if BUILDFLAG(IS_ANDROID)
    bnpl_strategy_ = std::make_unique<AndroidBnplStrategy>();
#else   // !BUILDFLAG(IS_ANDROID)
    bnpl_strategy_ = std::make_unique<DesktopBnplStrategy>();
#endif  // BUILDFLAG(IS_ANDROID)
  }
  return bnpl_strategy_.get();
}

BnplUiDelegate* ChromePaymentsAutofillClient::GetBnplUiDelegate() {
  if (!bnpl_ui_delegate_) {
#if BUILDFLAG(IS_ANDROID)
    bnpl_ui_delegate_ = std::make_unique<AndroidBnplUiDelegate>(this);
#else   // !BUILDFLAG(IS_ANDROID)
    bnpl_ui_delegate_ = std::make_unique<DesktopBnplUiDelegate>(&client_.get());
#endif  // BUILDFLAG(IS_ANDROID)
  }
  return bnpl_ui_delegate_.get();
}

#if BUILDFLAG(IS_ANDROID)
AutofillMessageController&
ChromePaymentsAutofillClient::GetAutofillMessageController() {
  if (!autofill_message_controller_) {
    autofill_message_controller_ =
        std::make_unique<AutofillMessageController>(web_contents());
  }

  return *autofill_message_controller_;
}

TouchToFillPaymentMethodController*
ChromePaymentsAutofillClient::GetTouchToFillPaymentMethodController() {
  return touch_to_fill_payment_method_controller_.get();
}
#endif

AutofillProgressDialogControllerImpl*
ChromePaymentsAutofillClient::AutofillProgressDialogControllerForTesting() {
  return autofill_progress_dialog_controller_.get();
}

std::unique_ptr<CardUnmaskPromptControllerImpl>
ChromePaymentsAutofillClient::ExtractCardUnmaskControllerForTesting() {
  return std::move(unmask_controller_);
}
void ChromePaymentsAutofillClient::SetCardUnmaskControllerForTesting(
    std::unique_ptr<CardUnmaskPromptControllerImpl> test_controller) {
  unmask_controller_ = std::move(test_controller);
}

#if BUILDFLAG(IS_ANDROID)
void ChromePaymentsAutofillClient::
    SetAutofillSaveCardBottomSheetBridgeForTesting(
        std::unique_ptr<AutofillSaveCardBottomSheetBridge>
            autofill_save_card_bottom_sheet_bridge) {
  autofill_save_card_bottom_sheet_bridge_ =
      std::move(autofill_save_card_bottom_sheet_bridge);
}

void ChromePaymentsAutofillClient::
    SetAutofillSaveIbanBottomSheetBridgeForTesting(
        std::unique_ptr<AutofillSaveIbanBottomSheetBridge>
            autofill_save_iban_bottom_sheet_bridge) {
  autofill_save_iban_bottom_sheet_bridge_ =
      std::move(autofill_save_iban_bottom_sheet_bridge);
}

void ChromePaymentsAutofillClient::SetAutofillMessageControllerForTesting(
    std::unique_ptr<AutofillMessageController> autofill_message_controller) {
  autofill_message_controller_ = std::move(autofill_message_controller);
}

void ChromePaymentsAutofillClient::
    SetTouchToFillPaymentMethodControllerForTesting(
        std::unique_ptr<TouchToFillPaymentMethodController>
            touch_to_fill_payment_method_controller) {
  touch_to_fill_payment_method_controller_ =
      std::move(touch_to_fill_payment_method_controller);
}
#endif  // #if BUILDFLAG(IS_ANDROID)

void ChromePaymentsAutofillClient::SetRiskDataForTesting(
    const std::string& risk_data) {
  risk_data_ = risk_data;
}

void ChromePaymentsAutofillClient::SetCachedRiskDataLoadedCallbackForTesting(
    base::OnceCallback<void(const std::string&)>
        cached_risk_data_loaded_callback_for_testing) {
  cached_risk_data_loaded_callback_for_testing_ =
      std::move(cached_risk_data_loaded_callback_for_testing);
}

std::u16string ChromePaymentsAutofillClient::GetAccountHolderName() const {
  if (!web_contents()) {
    return std::u16string();
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!profile) {
    return std::u16string();
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return std::u16string();
  }
  AccountInfo primary_account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  return base::UTF8ToUTF16(primary_account_info.full_name);
}

void ChromePaymentsAutofillClient::OnRiskDataLoaded(
    base::OnceCallback<void(const std::string&)> callback,
    base::TimeTicks start_time,
    const std::string& risk_data) {
  autofill_metrics::LogRiskDataLoadingLatency(base::TimeTicks::Now() -
                                              start_time);
  risk_data_ = risk_data;
  std::move(callback).Run(risk_data_);
}

}  // namespace autofill::payments
