// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_local_card_migration_manager.h"

#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

TestLocalCardMigrationManager::TestLocalCardMigrationManager(
    AutofillDriver* driver,
    AutofillClient* client,
    payments::TestPaymentsClient* payments_client,
    TestPersonalDataManager* personal_data_manager)
    : LocalCardMigrationManager(client,
                                payments_client,
                                "en-US",
                                personal_data_manager),
      personal_data_manager_(personal_data_manager) {}

TestLocalCardMigrationManager::~TestLocalCardMigrationManager() {}

bool TestLocalCardMigrationManager::IsCreditCardMigrationEnabled() {
  return payments::GetBillingCustomerId(personal_data_manager_) != 0;
}

bool TestLocalCardMigrationManager::LocalCardMigrationWasTriggered() {
  return local_card_migration_was_triggered_;
}

bool TestLocalCardMigrationManager::IntermediatePromptWasShown() {
  return intermediate_prompt_was_shown_;
}

bool TestLocalCardMigrationManager::MainPromptWasShown() {
  return main_prompt_was_shown_;
}

void TestLocalCardMigrationManager::
    OnUserAcceptedIntermediateMigrationDialog() {
  intermediate_prompt_was_shown_ = true;
  LocalCardMigrationManager::OnUserAcceptedIntermediateMigrationDialog();
}

void TestLocalCardMigrationManager::OnUserAcceptedMainMigrationDialog(
    const std::vector<std::string>& selected_cards) {
  main_prompt_was_shown_ = true;
  LocalCardMigrationManager::OnUserAcceptedMainMigrationDialog(selected_cards);
}

void TestLocalCardMigrationManager::ResetSyncState(
    AutofillSyncSigninState sync_state) {
  personal_data_manager_->SetSyncAndSignInState(sync_state);
}

void TestLocalCardMigrationManager::OnDidGetUploadDetails(
    bool is_from_settings_page,
    AutofillClient::PaymentsRpcResult result,
    const std::u16string& context_token,
    std::unique_ptr<base::Value::Dict> legal_message,
    std::vector<std::pair<int, int>> supported_bin_ranges) {
  if (result == AutofillClient::PaymentsRpcResult::kSuccess) {
    local_card_migration_was_triggered_ = true;
  }
  LocalCardMigrationManager::OnDidGetUploadDetails(
      is_from_settings_page, result, context_token, std::move(legal_message),
      supported_bin_ranges);
}

}  // namespace autofill
