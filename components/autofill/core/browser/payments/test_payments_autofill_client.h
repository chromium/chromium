// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_AUTOFILL_CLIENT_H_

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/mock_iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test/mock_iban_manager.h"
#include "components/autofill/core/browser/payments/test/mock_mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/test/mock_multiple_request_payments_network_interface.h"
#include "components/autofill/core/browser/payments/test/mock_save_and_fill_manager.h"
#include "components/autofill/core/browser/payments/test/test_credit_card_risk_based_authenticator.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/single_field_fillers/payments/mock_merchant_promo_code_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#include "components/autofill/core/browser/ui/payments/bnpl_ui_delegate.h"
#include "components/autofill/core/common/autofill_prefs.h"

#if !BUILDFLAG(IS_IOS)
namespace webauthn {
class InternalAuthenticator;
}
#endif  // !BUILDFLAG(IS_IOS)

namespace autofill {

class AutofillClient;
#if !BUILDFLAG(IS_IOS)
class AutofillDriver;
#endif  // !BUILDFLAG(IS_IOS)
class AutofillProgressDialogController;
class BnplIssuer;
struct BnplTosModel;
class CardUnmaskOtpInputDialogController;
class CardUnmaskPromptController;
class CreditCardCvcAuthenticator;
class TouchToFillDelegate;

namespace payments {

struct BnplIssuerContext;
class BnplStrategy;
class PaymentsWindowManager;

// This class is for easier writing of tests. It is owned by TestAutofillClient.
class TestPaymentsAutofillClient : public PaymentsAutofillClient {
 public:
  explicit TestPaymentsAutofillClient(AutofillClient* client);
  TestPaymentsAutofillClient(const TestPaymentsAutofillClient&) = delete;
  TestPaymentsAutofillClient& operator=(const TestPaymentsAutofillClient&) =
      delete;
  ~TestPaymentsAutofillClient() override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  // PaymentsAutofillClient:
#if BUILDFLAG(IS_ANDROID)
  AutofillSaveCardBottomSheetBridge*
  GetOrCreateAutofillSaveCardBottomSheetBridge() override;
  AutofillSaveIbanBottomSheetBridge*
  GetOrCreateAutofillSaveIbanBottomSheetBridge() override;
#endif
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void ShowWebauthnOfferDialog(
      WebauthnDialogCallback offer_dialog_callback) override;
  void ShowWebauthnVerifyPendingDialog(
      WebauthnDialogCallback verify_pending_dialog_callback) override;
  void UpdateWebauthnOfferDialogWithError() override;
  bool CloseWebauthnDialog() override;
  void HideVirtualCardEnrollBubbleAndIconIfVisible() override;
#else   // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback) override;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  bool HasCreditCardScanFeature() const override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  bool LocalCardSaveIsSupported() override;
  void ShowSaveCreditCardLocally(const CreditCard& card,
                                 SaveCreditCardOptions options,
                                 LocalSaveCardPromptCallback callback) override;
  void ShowSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) override;
  void CreditCardUploadCompleted(PaymentsRpcResult result,
                                 std::optional<OnConfirmationClosedCallback>
                                     on_confirmation_closed_callback) override;
  void HideSaveCardPrompt() override;
  void ShowVirtualCardEnrollDialog(
      const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback) override;
  void VirtualCardEnrollCompleted(PaymentsRpcResult result) override;
  void OnCardDataAvailable(
      const FilledCardInformationBubbleOptions& options) override;
  void ConfirmSaveIbanLocally(
      const Iban& iban,
      bool should_show_prompt,
      PaymentsAutofillClient::SaveIbanPromptCallback callback) override;
  void ConfirmUploadIbanToCloud(
      const Iban& iban,
      LegalMessageLines legal_message_lines,
      bool should_show_prompt,
      PaymentsAutofillClient::SaveIbanPromptCallback callback) override;
  void IbanUploadCompleted(bool iban_saved, bool hit_max_strikes) override;
  void ShowAutofillProgressDialog(
      AutofillProgressDialogType autofill_progress_dialog_type,
      base::OnceClosure cancel_callback) override;
  void CloseAutofillProgressDialog(
      bool show_confirmation_before_closing,
      base::OnceClosure no_user_perceived_authentication_callback) override;
  void ShowCardUnmaskOtpInputDialog(
      CreditCard::RecordType card_type,
      const CardUnmaskChallengeOption& challenge_option,
      base::WeakPtr<OtpUnmaskDelegate> delegate) override;
  void OnUnmaskOtpVerificationResult(OtpUnmaskResult unmask_result) override;
  void ShowUnmaskAuthenticatorSelectionDialog(
      const std::vector<CardUnmaskChallengeOption>& challenge_options,
      base::OnceCallback<void(const std::string&)>
          confirm_unmask_challenge_option_callback,
      base::OnceClosure cancel_unmasking_closure) override;
  void DismissUnmaskAuthenticatorSelectionDialog(bool server_success) override;
  PaymentsNetworkInterface* GetPaymentsNetworkInterface() override;
  MockMultipleRequestPaymentsNetworkInterface*
  GetMultipleRequestPaymentsNetworkInterface() override;
  void ShowAutofillErrorDialog(AutofillErrorDialogContext context) override;
  PaymentsWindowManager* GetPaymentsWindowManager() override;
  void ShowUnmaskPrompt(
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
#if BUILDFLAG(IS_IOS)
  std::unique_ptr<AutofillProgressDialogController> ExtractProgressDialogModel()
      override;
  std::unique_ptr<CardUnmaskOtpInputDialogController>
  ExtractOtpInputDialogModel() override;
  CardUnmaskPromptController* GetCardUnmaskPromptModel() override;
#endif
  VirtualCardEnrollmentManager* GetVirtualCardEnrollmentManager() override;
  CreditCardCvcAuthenticator& GetCvcAuthenticator() override;
  CreditCardOtpAuthenticator* GetOtpAuthenticator() override;
  TestCreditCardRiskBasedAuthenticator* GetRiskBasedAuthenticator() override;
  bool IsRiskBasedAuthEffectivelyAvailable() const override;
  bool IsMandatoryReauthEnabled() override;
  void ShowMandatoryReauthOptInPrompt(
      base::OnceClosure accept_mandatory_reauth_callback,
      base::OnceClosure cancel_mandatory_reauth_callback,
      base::RepeatingClosure close_mandatory_reauth_callback) override;
  void ShowMandatoryReauthOptInConfirmation() override;
  bool IsAutofillPaymentMethodsEnabled() const final;
  void DisablePaymentsAutofill() final;
  MockIbanManager* GetIbanManager() override;
  MockIbanAccessManager* GetIbanAccessManager() override;
  MockMerchantPromoCodeManager* GetMerchantPromoCodeManager() override;
  void OpenPromoCodeOfferDetailsURL(const GURL& url) override;
  AutofillOfferManager* GetAutofillOfferManager() override;
  void UpdateOfferNotification(
      const AutofillOfferData& offer,
      const OfferNotificationOptions& options) override;
  void DismissOfferNotification() override;
  bool ShowTouchToFillCreditCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const Suggestion> suggestions) override;
  bool ShowTouchToFillIban(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const autofill::Iban> ibans_to_suggest) override;
  bool ShowTouchToFillLoyaltyCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      std::vector<autofill::LoyaltyCard> loyalty_cards_to_suggest) override;
  bool OnPurchaseAmountExtracted(
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      std::optional<int64_t> extracted_amount,
      bool is_amount_supported_by_any_issuer,
      const std::optional<std::string>& app_locale,
      base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) override;
  bool ShowTouchToFillProgress(base::OnceClosure cancel_callback) override;
  bool ShowTouchToFillBnplIssuers(
      base::span<const payments::BnplIssuerContext> bnpl_issuer_contexts,
      const std::string& app_locale,
      base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
      base::OnceClosure cancel_callback) override;
  bool ShowTouchToFillError(const AutofillErrorDialogContext& context) override;
  bool ShowTouchToFillBnplTos(BnplTosModel bnpl_tos_model,
                              base::OnceClosure accept_callback,
                              base::OnceClosure cancel_callback) override;
  void HideTouchToFillPaymentMethod() override;
  void SetTouchToFillVisible(bool visible) override;
  PaymentsDataManager& GetPaymentsDataManager() final;
#if !BUILDFLAG(IS_IOS)
  std::unique_ptr<webauthn::InternalAuthenticator>
  CreateCreditCardInternalAuthenticator(AutofillDriver* driver) override;
#endif
  MockMandatoryReauthManager* GetOrCreatePaymentsMandatoryReauthManager()
      override;
  MockSaveAndFillManager* GetSaveAndFillManager() override;
  void ShowCreditCardLocalSaveAndFillDialog(
      CardSaveAndFillDialogCallback callback) override;
  void ShowCreditCardUploadSaveAndFillDialog(
      const LegalMessageLines& legal_message_lines,
      CardSaveAndFillDialogCallback callback) override;
  void ShowCreditCardSaveAndFillPendingDialog() override;
  void HideCreditCardSaveAndFillDialog() override;
  bool IsTabModalPopupDeprecated() const override;
  BnplStrategy* GetBnplStrategy() override;
  BnplUiDelegate* GetBnplUiDelegate() override;

  // Begin TestPaymentsAutofillClient-specific section.

  void SetAutofillPaymentMethodsEnabled(bool autofill_payment_methods_enabled) {
    autofill_payment_methods_enabled_ = autofill_payment_methods_enabled;
    if (PrefService* prefs = client_->GetPrefs()) {
      prefs->SetBoolean(prefs::kAutofillCreditCardEnabled,
                        autofill_payment_methods_enabled);
    }
    if (!autofill_payment_methods_enabled) {
      // Credit card data is refreshed when this pref is changed.
      static_cast<TestPersonalDataManager&>(client_->GetPersonalDataManager())
          .test_payments_data_manager()
          .ClearCreditCards();
    }
  }

  bool GetMandatoryReauthOptInPromptWasShown();

  bool GetMandatoryReauthOptInPromptWasReshown();

#if BUILDFLAG(IS_ANDROID)
  // Set up a mock to simulate successful mandatory reauth when autofilling
  // payment methods.
  void SetUpDeviceBiometricAuthenticatorSuccessOnAutomotive();
#endif

  bool autofill_progress_dialog_shown() {
    return autofill_progress_dialog_shown_;
  }

  void set_payments_network_interface(
      std::unique_ptr<PaymentsNetworkInterface> payments_network_interface) {
    payments_network_interface_ = std::move(payments_network_interface);
  }

  void set_multiple_request_payments_network_interface(
      std::unique_ptr<MockMultipleRequestPaymentsNetworkInterface>
          multiple_request_payments_network_interface) {
    multiple_request_payments_network_interface_ =
        std::move(multiple_request_payments_network_interface);
  }

  bool autofill_error_dialog_shown() { return autofill_error_dialog_shown_; }

  bool show_otp_input_dialog() { return show_otp_input_dialog_; }

  void ResetShowOtpInputDialog() { show_otp_input_dialog_ = false; }

  bool ConfirmSaveIbanLocallyWasCalled() const {
    return confirm_save_iban_locally_called_;
  }

  bool offer_to_save_iban_bubble_was_shown() const {
    return offer_to_save_iban_bubble_was_shown_;
  }

  bool risk_data_loaded() const { return risk_data_loaded_; }
  void set_risk_data_loaded(bool risk_data_loaded) {
    risk_data_loaded_ = risk_data_loaded;
  }

  bool ConfirmUploadIbanToCloudWasCalled() const {
    return confirm_upload_iban_to_cloud_called_ &&
           !legal_message_lines_.empty();
  }

  AutofillProgressDialogType autofill_progress_dialog_type() const {
    return autofill_progress_dialog_type_;
  }

  const AutofillErrorDialogContext& autofill_error_dialog_context() {
    return autofill_error_dialog_context_;
  }

  void set_payments_window_manager(
      std::unique_ptr<PaymentsWindowManager> payments_window_manager) {
    payments_window_manager_ = std::move(payments_window_manager);
  }

  void set_virtual_card_enrollment_manager(
      std::unique_ptr<VirtualCardEnrollmentManager> vcem) {
    virtual_card_enrollment_manager_ = std::move(vcem);
  }

  void set_otp_authenticator(
      std::unique_ptr<CreditCardOtpAuthenticator> authenticator) {
    otp_authenticator_ = std::move(authenticator);
  }

  bool risk_based_authentication_invoked() {
    return risk_based_authenticator_ &&
           risk_based_authenticator_->authenticate_invoked();
  }

  void set_autofill_offer_manager(
      std::unique_ptr<AutofillOfferManager> autofill_offer_manager) {
    autofill_offer_manager_ = std::move(autofill_offer_manager);
  }

  bool unmask_authenticator_selection_dialog_shown() const {
    return unmask_authenticator_selection_dialog_shown_;
  }

  void set_is_tab_model_popup(bool is_tab_model_popup) {
    is_tab_model_popup_ = is_tab_model_popup;
  }

  void set_bnpl_ui_delegate(std::unique_ptr<BnplUiDelegate> bnpl_ui_delegate) {
    bnpl_ui_delegate_ = std::move(bnpl_ui_delegate);
  }

 private:
  const raw_ref<AutofillClient> client_;

  std::unique_ptr<PaymentsNetworkInterface> payments_network_interface_;

  std::unique_ptr<MockMultipleRequestPaymentsNetworkInterface>
      multiple_request_payments_network_interface_;

  bool autofill_progress_dialog_shown_ = false;

  bool autofill_error_dialog_shown_ = false;

  bool show_otp_input_dialog_ = false;

  bool confirm_save_iban_locally_called_ = false;
  bool confirm_upload_iban_to_cloud_called_ = false;

  // Populated if IBAN save was offered. True if bubble was shown, false
  // otherwise.
  bool offer_to_save_iban_bubble_was_shown_ = false;

  // True if LoadRiskData() was called, false otherwise.
  bool risk_data_loaded_ = false;

  bool is_tab_model_popup_ = false;

  AutofillProgressDialogType autofill_progress_dialog_type_ =
      AutofillProgressDialogType::kServerCardUnmaskProgressDialog;

  LegalMessageLines legal_message_lines_;

  // Context parameters that are used to display an error dialog during card
  // number retrieval. This context will have information that the autofill
  // error dialog uses to display a dialog specific to the error that occurred.
  // An example of where this dialog is used is if an error occurs during
  // virtual card number retrieval, as this context is then filled with fields
  // specific to the type of error that occurred, and then based on the contents
  // of this context the dialog is shown.
  AutofillErrorDialogContext autofill_error_dialog_context_;

  std::unique_ptr<PaymentsWindowManager> payments_window_manager_;

  // `virtual_card_enrollment_manager_` must be destroyed before
  // `payments_network_interface_` because the former keeps a reference to the
  // latter.
  // TODO(crbug.com/41489024): Remove the reference to
  // `payments_network_interface_` in `virtual_card_enrollment_manager_`.
  std::unique_ptr<VirtualCardEnrollmentManager>
      virtual_card_enrollment_manager_;

  std::unique_ptr<CreditCardCvcAuthenticator> cvc_authenticator_;

  std::unique_ptr<CreditCardOtpAuthenticator> otp_authenticator_;

  std::unique_ptr<TestCreditCardRiskBasedAuthenticator>
      risk_based_authenticator_;

  // Populated if mandatory re-auth opt-in was offered or re-offered,
  // respectively.
  bool mandatory_reauth_opt_in_prompt_was_shown_ = false;
  bool mandatory_reauth_opt_in_prompt_was_reshown_ = false;

  bool unmask_authenticator_selection_dialog_shown_ = false;

  bool autofill_payment_methods_enabled_ = true;
  bool autofill_payment_methods_supported_ = true;

  std::unique_ptr<MockIbanManager> mock_iban_manager_;

  std::unique_ptr<MockIbanAccessManager> mock_iban_access_manager_;

  std::unique_ptr<MockSaveAndFillManager> mock_save_and_fill_manager_;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Populated if name fix flow was offered. True if bubble was shown, false
  // otherwise.
  bool credit_card_name_fix_flow_bubble_was_shown_ = false;
#endif

  ::testing::NiceMock<MockMerchantPromoCodeManager>
      mock_merchant_promo_code_manager_;
  std::unique_ptr<AutofillOfferManager> autofill_offer_manager_;
  std::unique_ptr<MockMandatoryReauthManager>
      mock_payments_mandatory_reauth_manager_;

  // The BnplStrategy used to determine the next step in a BNPL flow depending
  // on the platform.
  // Lazily initialized: access only through `GetBnplStrategy()`.
  std::unique_ptr<BnplStrategy> bnpl_strategy_;

  // The BnplUiDelegate used to handle the UI in a BNPL flow depending on the
  // platform.
  // Lazily initialized: access only through `GetBnplUiDelegate()`.
  std::unique_ptr<BnplUiDelegate> bnpl_ui_delegate_;
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_AUTOFILL_CLIENT_H_
