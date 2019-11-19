// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_local_card_migration_manager.h"

#include "components/autofill/core/browser/autofill_metrics.h"
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
  bool has_google_payments_account =
      (payments::GetBillingCustomerId(personal_data_manager_) != 0);

  bool sync_feature_enabled =
      (personal_data_manager_->GetSyncSigninState() ==
       AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled);

  return has_google_payments_account &&
         (sync_feature_enabled ||
          base::FeatureList::IsEnabled(
              features::kAutofillEnableLocalCardMigrationForNonSyncUser));
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
    const base::string16& context_token,
    std::unique_ptr<base::Value> legal_message,
    std::vector<std::pair<int, int>> supported_bin_ranges) {
  if (result == AutofillClient::SUCCESS) {
    local_card_migration_was_triggered_ = true;
  }
  LocalCardMigrationManager::OnDidGetUploadDetails(
      is_from_settings_page, result, context_token, std::move(legal_message),
      supported_bin_ranges);
}

}  // namespace autofill
