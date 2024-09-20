// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_AUTOFILL_CLIENT_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/mock_merchant_promo_code_manager.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/mock_iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test/mock_iban_manager.h"
#include "components/autofill/core/browser/payments/test/mock_mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/test/test_credit_card_risk_based_authenticator.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/ui/suggestion.h"

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
class CreditCardCvcAuthenticator;
class CreditCardOtpAuthenticator;
class MerchantPromoCodeManager;
class TouchToFillDelegate;
class VirtualCardEnrollmentManager;

namespace payments {

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
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) override;
  void ConfirmMigrateLocalCardToCloud(
      const LegalMessageLines& legal_message_lines,
      const std::string& user_email,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      PaymentsAutofillClient::LocalCardMigrationCallback
          start_migrating_cards_callback) override;
  void ConfirmSaveIbanLocally(
      const Iban& iban,
      bool should_show_prompt,
      PaymentsAutofillClient::SaveIbanPromptCallback callback) override;
  void ConfirmUploadIbanToCloud(
      const Iban& iban,
      LegalMessageLines legal_message_lines,
      bool should_show_prompt,
      PaymentsAutofillClient::SaveIbanPromptCallback callback) override;
  bool CloseWebauthnDialog() override;
#else   // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          callback) override;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  TestPaymentsNetworkInterface* GetPaymentsNetworkInterface() override;
  void ShowAutofillProgressDialog(
      AutofillProgressDialogType autofill_progress_dialog_type,
      base::OnceClosure cancel_callback) override;
  void CloseAutofillProgressDialog(
      bool show_confirmation_before_closing,
      base::OnceClosure no_user_perceived_authentication_callback) override;
  void ShowAutofillErrorDialog(AutofillErrorDialogContext context) override;
  void ShowCardUnmaskOtpInputDialog(
      const CardUnmaskChallengeOption& challenge_option,
      base::WeakPtr<OtpUnmaskDelegate> delegate) override;
  PaymentsWindowManager* GetPaymentsWindowManager() override;
  VirtualCardEnrollmentManager* GetVirtualCardEnrollmentManager() override;
  CreditCardCvcAuthenticator& GetCvcAuthenticator() override;
  CreditCardOtpAuthenticator* GetOtpAuthenticator() override;
  TestCreditCardRiskBasedAuthenticator* GetRiskBasedAuthenticator() override;
  void ShowMandatoryReauthOptInPrompt(
      base::OnceClosure accept_mandatory_reauth_callback,
      base::OnceClosure cancel_mandatory_reauth_callback,
      base::RepeatingClosure close_mandatory_reauth_callback) override;
  MockIbanManager* GetIbanManager() override;
  MockIbanAccessManager* GetIbanAccessManager() override;
  void ShowMandatoryReauthOptInConfirmation() override;
  MerchantPromoCodeManager* GetMerchantPromoCodeManager() override;
  AutofillOfferManager* GetAutofillOfferManager() override;
  bool ShowTouchToFillCreditCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const autofill::CreditCard> cards_to_suggest,
      base::span<const Suggestion> suggestions) override;
#if !BUILDFLAG(IS_IOS)
  std::unique_ptr<webauthn::InternalAuthenticator>
  CreateCreditCardInternalAuthenticator(AutofillDriver* driver) override;
#endif
  MockMandatoryReauthManager* GetOrCreatePaymentsMandatoryReauthManager()
      override;

  bool GetMandatoryReauthOptInPromptWasShown();

  bool GetMandatoryReauthOptInPromptWasReshown();

  void set_migration_card_selections(
      const std::vector<std::string>& migration_card_selection) {
    migration_card_selection_ = migration_card_selection;
  }

  bool autofill_progress_dialog_shown() {
    return autofill_progress_dialog_shown_;
  }

  void set_test_payments_network_interface(
      std::unique_ptr<TestPaymentsNetworkInterface>
          payments_network_interface) {
    payments_network_interface_ = std::move(payments_network_interface);
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

  bool ConfirmUploadIbanToCloudWasCalled() const {
    return confirm_upload_iban_to_cloud_called_ &&
           !legal_message_lines_.empty();
  }

  const AutofillErrorDialogContext& autofill_error_dialog_context() {
    return autofill_error_dialog_context_;
  }

  void set_payments_window_manager(
      std::unique_ptr<PaymentsWindowManager> payments_window_manager) {
    payments_window_manager_ = std::move(payments_window_manager);
  }

  void set_virtual_card_enrollment_manager(
      std::unique_ptr<VirtualCardEnrollmentManager> vcem);

  void set_otp_authenticator(
      std::unique_ptr<CreditCardOtpAuthenticator> authenticator);

  bool risk_based_authentication_invoked() {
    return risk_based_authenticator_ &&
           risk_based_authenticator_->authenticate_invoked();
  }

  MockMerchantPromoCodeManager* GetMockMerchantPromoCodeManager();

  void set_autofill_offer_manager(
      std::unique_ptr<AutofillOfferManager> autofill_offer_manager) {
    autofill_offer_manager_ = std::move(autofill_offer_manager);
  }

#if BUILDFLAG(IS_ANDROID)
  // Set up a mock to simulate successful mandatory reauth when autofilling
  // payment methods.
  void SetUpDeviceBiometricAuthenticatorSuccessOnAutomotive();
#endif

 private:
  const raw_ref<AutofillClient> client_;

  std::unique_ptr<TestPaymentsNetworkInterface> payments_network_interface_;

  std::vector<std::string> migration_card_selection_;

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

  std::unique_ptr<MockIbanManager> mock_iban_manager_;

  std::unique_ptr<MockIbanAccessManager> mock_iban_access_manager_;

  // Populated if name fix flow was offered. True if bubble was shown, false
  // otherwise.
  bool credit_card_name_fix_flow_bubble_was_shown_ = false;

  ::testing::NiceMock<MockMerchantPromoCodeManager>
      mock_merchant_promo_code_manager_;

  std::unique_ptr<AutofillOfferManager> autofill_offer_manager_;

  std::unique_ptr<MockMandatoryReauthManager>
      mock_payments_mandatory_reauth_manager_;
};

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_AUTOFILL_CLIENT_H_
