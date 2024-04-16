// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"

#include "base/functional/callback.h"
#include "components/autofill/core/browser/payments/test/mock_payments_window_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace autofill::payments {

TestPaymentsAutofillClient::TestPaymentsAutofillClient() = default;

TestPaymentsAutofillClient::~TestPaymentsAutofillClient() = default;

void TestPaymentsAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run("some risk data");
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void TestPaymentsAutofillClient::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {
  std::move(show_migration_dialog_closure).Run();
}

void TestPaymentsAutofillClient::ConfirmMigrateLocalCardToCloud(
    const LegalMessageLines& legal_message_lines,
    const std::string& user_email,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    PaymentsAutofillClient::LocalCardMigrationCallback
        start_migrating_cards_callback) {
  // If `migration_card_selection_` hasn't been preset by tests, default to
  // selecting all migratable cards.
  if (migration_card_selection_.empty()) {
    for (MigratableCreditCard card : migratable_credit_cards) {
      migration_card_selection_.push_back(card.credit_card().guid());
    }
  }
  std::move(start_migrating_cards_callback).Run(migration_card_selection_);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

void TestPaymentsAutofillClient::ShowAutofillProgressDialog(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  autofill_progress_dialog_shown_ = true;
}

void TestPaymentsAutofillClient::CloseAutofillProgressDialog(
    bool show_confirmation_before_closing,
    base::OnceClosure no_user_perceived_authentication_callback) {
  if (no_user_perceived_authentication_callback) {
    std::move(no_user_perceived_authentication_callback).Run();
  }
}

TestPaymentsNetworkInterface*
TestPaymentsAutofillClient::GetPaymentsNetworkInterface() {
  return payments_network_interface_.get();
}

void TestPaymentsAutofillClient::ShowAutofillErrorDialog(
    AutofillErrorDialogContext context) {
  autofill_error_dialog_shown_ = true;
  autofill_error_dialog_context_ = std::move(context);
}

void TestPaymentsAutofillClient::ShowCardUnmaskOtpInputDialog(
    const CardUnmaskChallengeOption& challenge_option,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {
  show_otp_input_dialog_ = true;
}

PaymentsWindowManager* TestPaymentsAutofillClient::GetPaymentsWindowManager() {
  if (!payments_window_manager_) {
    payments_window_manager_ =
        std::make_unique<testing::NiceMock<MockPaymentsWindowManager>>();
  }
  return payments_window_manager_.get();
}

}  // namespace autofill::payments
