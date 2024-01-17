// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"

#include "base/functional/callback.h"

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
    payments::PaymentsAutofillClient::LocalCardMigrationCallback
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

}  // namespace autofill::payments
