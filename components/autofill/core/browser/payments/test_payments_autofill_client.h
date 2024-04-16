// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_AUTOFILL_CLIENT_H_

#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"

namespace autofill::payments {

class PaymentsWindowManager;

// This class is for easier writing of tests. It is owned by TestAutofillClient.
class TestPaymentsAutofillClient : public PaymentsAutofillClient {
 public:
  TestPaymentsAutofillClient();
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

  const AutofillErrorDialogContext& autofill_error_dialog_context() {
    return autofill_error_dialog_context_;
  }

  void set_payments_window_manager(
      std::unique_ptr<PaymentsWindowManager> payments_window_manager) {
    payments_window_manager_ = std::move(payments_window_manager);
  }

 private:
  std::unique_ptr<TestPaymentsNetworkInterface> payments_network_interface_;

  std::vector<std::string> migration_card_selection_;

  bool autofill_progress_dialog_shown_ = false;

  bool autofill_error_dialog_shown_ = false;

  bool show_otp_input_dialog_ = false;

  // Context parameters that are used to display an error dialog during card
  // number retrieval. This context will have information that the autofill
  // error dialog uses to display a dialog specific to the error that occurred.
  // An example of where this dialog is used is if an error occurs during
  // virtual card number retrieval, as this context is then filled with fields
  // specific to the type of error that occurred, and then based on the contents
  // of this context the dialog is shown.
  AutofillErrorDialogContext autofill_error_dialog_context_;

  std::unique_ptr<PaymentsWindowManager> payments_window_manager_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_PAYMENTS_AUTOFILL_CLIENT_H_
