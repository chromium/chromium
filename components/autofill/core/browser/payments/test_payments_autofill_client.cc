// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"

#include <memory>

#include "base/android/device_info.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_delegate.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/bnpl_strategy.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/test/mock_payments_window_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/payments/bnpl_ui_delegate.h"
#include "components/autofill/core/common/autofill_prefs.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/test/gmock_callback_support.h"
#include "components/autofill/core/browser/payments/android_bnpl_strategy.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/test_internal_authenticator.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#endif  // !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/desktop_bnpl_strategy.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

namespace autofill::payments {

using ::base::test::RunOnceCallbackRepeatedly;
using ::testing::NiceMock;
using ::testing::Return;

TestPaymentsAutofillClient::TestPaymentsAutofillClient(AutofillClient* client)
    : client_(CHECK_DEREF(client)),
      mock_save_and_fill_manager_(
          std::make_unique<NiceMock<MockSaveAndFillManager>>()),
      mock_merchant_promo_code_manager_(
          &client_->GetPersonalDataManager().payments_data_manager()) {}

TestPaymentsAutofillClient::~TestPaymentsAutofillClient() = default;

void TestPaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  risk_data_loaded_ = true;
  std::move(callback).Run("some risk data");
}

#if BUILDFLAG(IS_ANDROID)
AutofillSaveCardBottomSheetBridge*
TestPaymentsAutofillClient::GetOrCreateAutofillSaveCardBottomSheetBridge() {
  return nullptr;
}

AutofillSaveIbanBottomSheetBridge*
TestPaymentsAutofillClient::GetOrCreateAutofillSaveIbanBottomSheetBridge() {
  return nullptr;
}
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void TestPaymentsAutofillClient::ShowWebauthnOfferDialog(
    WebauthnDialogCallback offer_dialog_callback) {}

void TestPaymentsAutofillClient::ShowWebauthnVerifyPendingDialog(
    WebauthnDialogCallback verify_pending_dialog_callback) {}

void TestPaymentsAutofillClient::UpdateWebauthnOfferDialogWithError() {}

bool TestPaymentsAutofillClient::CloseWebauthnDialog() {
  return true;
}

void TestPaymentsAutofillClient::HideVirtualCardEnrollBubbleAndIconIfVisible() {
}

#else   // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void TestPaymentsAutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const std::u16string&)> callback) {
  credit_card_name_fix_flow_bubble_was_shown_ = true;
  std::move(callback).Run(std::u16string(u"Gaia Name"));
}

void TestPaymentsAutofillClient::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
        callback) {
  credit_card_name_fix_flow_bubble_was_shown_ = true;
  std::move(callback).Run(
      std::u16string(u"03"),
      std::u16string(base::ASCIIToUTF16(test::NextYear().c_str())));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

bool TestPaymentsAutofillClient::HasCreditCardScanFeature() const {
  return false;
}

void TestPaymentsAutofillClient::ScanCreditCard(
    CreditCardScanCallback callback) {}

bool TestPaymentsAutofillClient::LocalCardSaveIsSupported() {
  return true;
}

void TestPaymentsAutofillClient::ShowSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {}

void TestPaymentsAutofillClient::ShowSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {}

void TestPaymentsAutofillClient::CreditCardUploadCompleted(
    PaymentsRpcResult result,
    std::optional<OnConfirmationClosedCallback>
        on_confirmation_closed_callback) {}

void TestPaymentsAutofillClient::HideSaveCardPrompt() {}

void TestPaymentsAutofillClient::ShowVirtualCardEnrollDialog(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {}

void TestPaymentsAutofillClient::VirtualCardEnrollCompleted(
    PaymentsRpcResult result) {}

void TestPaymentsAutofillClient::OnCardDataAvailable(
    const FilledCardInformationBubbleOptions& options) {}

void TestPaymentsAutofillClient::ConfirmSaveIbanLocally(
    const Iban& iban,
    bool should_show_prompt,
    payments::PaymentsAutofillClient::SaveIbanPromptCallback callback) {
  confirm_save_iban_locally_called_ = true;
  offer_to_save_iban_bubble_was_shown_ = should_show_prompt;
}

void TestPaymentsAutofillClient::ConfirmUploadIbanToCloud(
    const Iban& iban,
    LegalMessageLines legal_message_lines,
    bool should_show_prompt,
    payments::PaymentsAutofillClient::SaveIbanPromptCallback callback) {
  confirm_upload_iban_to_cloud_called_ = true;
  legal_message_lines_ = std::move(legal_message_lines);
  offer_to_save_iban_bubble_was_shown_ = should_show_prompt;
}

void TestPaymentsAutofillClient::IbanUploadCompleted(bool iban_saved,
                                                     bool hit_max_strikes) {}

void TestPaymentsAutofillClient::ShowAutofillProgressDialog(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  autofill_progress_dialog_shown_ = true;
  autofill_progress_dialog_type_ = autofill_progress_dialog_type;
}

void TestPaymentsAutofillClient::CloseAutofillProgressDialog(
    bool show_confirmation_before_closing,
    base::OnceClosure no_user_perceived_authentication_callback) {
  if (no_user_perceived_authentication_callback) {
    std::move(no_user_perceived_authentication_callback).Run();
  }
}

void TestPaymentsAutofillClient::ShowCardUnmaskOtpInputDialog(
    CreditCard::RecordType card_type,
    const CardUnmaskChallengeOption& challenge_option,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {
  show_otp_input_dialog_ = true;
}

void TestPaymentsAutofillClient::OnUnmaskOtpVerificationResult(
    OtpUnmaskResult unmask_result) {}

void TestPaymentsAutofillClient::ShowUnmaskAuthenticatorSelectionDialog(
    const std::vector<CardUnmaskChallengeOption>& challenge_options,
    base::OnceCallback<void(const std::string&)>
        confirm_unmask_challenge_option_callback,
    base::OnceClosure cancel_unmasking_closure) {
  unmask_authenticator_selection_dialog_shown_ = true;
}

void TestPaymentsAutofillClient::DismissUnmaskAuthenticatorSelectionDialog(
    bool server_success) {}

PaymentsNetworkInterface*
TestPaymentsAutofillClient::GetPaymentsNetworkInterface() {
  return payments_network_interface_.get();
}

MockMultipleRequestPaymentsNetworkInterface*
TestPaymentsAutofillClient::GetMultipleRequestPaymentsNetworkInterface() {
  return multiple_request_payments_network_interface_.get();
}

void TestPaymentsAutofillClient::ShowAutofillErrorDialog(
    AutofillErrorDialogContext context) {
  autofill_error_dialog_shown_ = true;
  autofill_error_dialog_context_ = std::move(context);
}

PaymentsWindowManager* TestPaymentsAutofillClient::GetPaymentsWindowManager() {
  if (!payments_window_manager_) {
    payments_window_manager_ =
        std::make_unique<NiceMock<MockPaymentsWindowManager>>();
  }
  return payments_window_manager_.get();
}

void TestPaymentsAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {}

void TestPaymentsAutofillClient::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {}

#if BUILDFLAG(IS_IOS)
std::unique_ptr<AutofillProgressDialogController>
TestPaymentsAutofillClient::ExtractProgressDialogModel() {
  return nullptr;
}

std::unique_ptr<CardUnmaskOtpInputDialogController>
TestPaymentsAutofillClient::ExtractOtpInputDialogModel() {
  return nullptr;
}

CardUnmaskPromptController*
TestPaymentsAutofillClient::GetCardUnmaskPromptModel() {
  return nullptr;
}
#endif

VirtualCardEnrollmentManager*
TestPaymentsAutofillClient::GetVirtualCardEnrollmentManager() {
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

CreditCardCvcAuthenticator& TestPaymentsAutofillClient::GetCvcAuthenticator() {
  if (!cvc_authenticator_) {
    cvc_authenticator_ =
        std::make_unique<CreditCardCvcAuthenticator>(&client_.get());
  }
  return *cvc_authenticator_;
}

CreditCardOtpAuthenticator* TestPaymentsAutofillClient::GetOtpAuthenticator() {
  if (!otp_authenticator_) {
    otp_authenticator_ =
        std::make_unique<CreditCardOtpAuthenticator>(&client_.get());
  }
  return otp_authenticator_.get();
}

TestCreditCardRiskBasedAuthenticator*
TestPaymentsAutofillClient::GetRiskBasedAuthenticator() {
  if (!risk_based_authenticator_) {
    risk_based_authenticator_ =
        std::make_unique<TestCreditCardRiskBasedAuthenticator>(&client_.get());
  }
  return risk_based_authenticator_.get();
}

bool TestPaymentsAutofillClient::IsRiskBasedAuthEffectivelyAvailable() const {
  return true;
}

bool TestPaymentsAutofillClient::IsMandatoryReauthEnabled() {
  return GetPaymentsDataManager().IsPaymentMethodsMandatoryReauthEnabled();
}

#if BUILDFLAG(IS_IOS)
bool TestPaymentsAutofillClient::IsUsingCustomCardIconEnabled() const {
  return true;
}
#endif  // BUILDFLAG(IS_IOS)

void TestPaymentsAutofillClient::ShowMandatoryReauthOptInPrompt(
    base::OnceClosure accept_mandatory_reauth_callback,
    base::OnceClosure cancel_mandatory_reauth_callback,
    base::RepeatingClosure close_mandatory_reauth_callback) {
  mandatory_reauth_opt_in_prompt_was_shown_ = true;
}

void TestPaymentsAutofillClient::ShowMandatoryReauthOptInConfirmation() {
  mandatory_reauth_opt_in_prompt_was_reshown_ = true;
}

bool TestPaymentsAutofillClient::IsAutofillPaymentMethodsEnabled() const {
  return autofill_payment_methods_enabled_ &&
         autofill_payment_methods_supported_;
}

void TestPaymentsAutofillClient::DisablePaymentsAutofill() {
  autofill_payment_methods_supported_ = false;
}

MockIbanManager* TestPaymentsAutofillClient::GetIbanManager() {
  if (!mock_iban_manager_) {
    mock_iban_manager_ = std::make_unique<NiceMock<MockIbanManager>>(
        &client_->GetPersonalDataManager().payments_data_manager());
  }
  return mock_iban_manager_.get();
}

MockIbanAccessManager* TestPaymentsAutofillClient::GetIbanAccessManager() {
  if (!mock_iban_access_manager_) {
    mock_iban_access_manager_ =
        std::make_unique<NiceMock<MockIbanAccessManager>>(&client_.get());
  }
  return mock_iban_access_manager_.get();
}

MockMerchantPromoCodeManager*
TestPaymentsAutofillClient::GetMerchantPromoCodeManager() {
  return &mock_merchant_promo_code_manager_;
}

void TestPaymentsAutofillClient::OpenPromoCodeOfferDetailsURL(const GURL& url) {
}

AutofillOfferManager* TestPaymentsAutofillClient::GetAutofillOfferManager() {
  return autofill_offer_manager_.get();
}

void TestPaymentsAutofillClient::UpdateOfferNotification(
    const AutofillOfferData& offer,
    const OfferNotificationOptions& options) {}

void TestPaymentsAutofillClient::DismissOfferNotification() {}

bool TestPaymentsAutofillClient::ShowTouchToFillCreditCard(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const Suggestion> suggestions) {
  return false;
}

bool TestPaymentsAutofillClient::ShowTouchToFillIban(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const Iban> ibans_to_suggest) {
  return false;
}

bool TestPaymentsAutofillClient::ShowTouchToFillLoyaltyCard(
    base::WeakPtr<TouchToFillDelegate> delegate,
    std::vector<LoyaltyCard> loyalty_cards_to_suggest) {
  return false;
}

bool TestPaymentsAutofillClient::OnPurchaseAmountExtracted(
    base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
    std::optional<int64_t> extracted_amount,
    bool is_amount_supported_by_any_issuer,
    const std::optional<std::string>& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  return false;
}

bool TestPaymentsAutofillClient::ShowTouchToFillProgress(
    base::OnceClosure cancel_callback) {
  return false;
}

bool TestPaymentsAutofillClient::ShowTouchToFillBnplIssuers(
    base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
    const std::string& app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  return false;
}

bool TestPaymentsAutofillClient::ShowTouchToFillError(
    const AutofillErrorDialogContext& context) {
  return false;
}

bool TestPaymentsAutofillClient::ShowTouchToFillBnplTos(
    BnplTosModel bnpl_tos_model,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback) {
  return false;
}

void TestPaymentsAutofillClient::HideTouchToFillPaymentMethod() {}

void TestPaymentsAutofillClient::SetTouchToFillVisible(bool visible) {}

PaymentsDataManager& TestPaymentsAutofillClient::GetPaymentsDataManager() {
  return client_->GetPersonalDataManager().payments_data_manager();
}

#if !BUILDFLAG(IS_IOS)
std::unique_ptr<webauthn::InternalAuthenticator>
TestPaymentsAutofillClient::CreateCreditCardInternalAuthenticator(
    AutofillDriver* driver) {
  return std::make_unique<TestInternalAuthenticator>();
}
#endif

MockMandatoryReauthManager*
TestPaymentsAutofillClient::GetOrCreatePaymentsMandatoryReauthManager() {
  if (!mock_payments_mandatory_reauth_manager_) {
    mock_payments_mandatory_reauth_manager_ =
        std::make_unique<NiceMock<payments::MockMandatoryReauthManager>>();
  }
  return mock_payments_mandatory_reauth_manager_.get();
}

MockSaveAndFillManager* TestPaymentsAutofillClient::GetSaveAndFillManager() {
  return mock_save_and_fill_manager_.get();
}

void TestPaymentsAutofillClient::ShowCreditCardLocalSaveAndFillDialog(
    CardSaveAndFillDialogCallback callback) {}

void TestPaymentsAutofillClient::ShowCreditCardUploadSaveAndFillDialog(
    const LegalMessageLines& legal_message_lines,
    CardSaveAndFillDialogCallback callback) {}

void TestPaymentsAutofillClient::ShowCreditCardSaveAndFillPendingDialog() {}

void TestPaymentsAutofillClient::HideCreditCardSaveAndFillDialog() {}

bool TestPaymentsAutofillClient::IsTabModalPopupDeprecated() const {
  return is_tab_model_popup_;
}

BnplStrategy* TestPaymentsAutofillClient::GetBnplStrategy() {
  if (!bnpl_strategy_) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    bnpl_strategy_ = std::make_unique<DesktopBnplStrategy>();
#elif BUILDFLAG(IS_ANDROID)
    bnpl_strategy_ = std::make_unique<AndroidBnplStrategy>();
#else   // BUILDFLAG(IS_IOS)
    bnpl_strategy_ = nullptr;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  }
  return bnpl_strategy_.get();
}

BnplUiDelegate* TestPaymentsAutofillClient::GetBnplUiDelegate() {
  return bnpl_ui_delegate_.get();
}

bool TestPaymentsAutofillClient::GetMandatoryReauthOptInPromptWasShown() {
  return mandatory_reauth_opt_in_prompt_was_shown_;
}

bool TestPaymentsAutofillClient::GetMandatoryReauthOptInPromptWasReshown() {
  return mandatory_reauth_opt_in_prompt_was_reshown_;
}

#if BUILDFLAG(IS_ANDROID)
void TestPaymentsAutofillClient::
    SetUpDeviceBiometricAuthenticatorSuccessOnAutomotive() {
  if (!base::android::device_info::is_automotive()) {
    return;
  }

  payments::MockMandatoryReauthManager& mandatory_reauth_manager =
      *GetOrCreatePaymentsMandatoryReauthManager();

  ON_CALL(mandatory_reauth_manager, GetAuthenticationMethod)
      .WillByDefault(
          Return(payments::MandatoryReauthAuthenticationMethod::kBiometric));
  ON_CALL(mandatory_reauth_manager, Authenticate)
      .WillByDefault(RunOnceCallbackRepeatedly<0>(true));
}
#endif

}  // namespace autofill::payments
