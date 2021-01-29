// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_LOCAL_CARD_MIGRATION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_LOCAL_CARD_MIGRATION_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"

namespace autofill {

namespace payments {
class TestPaymentsClient;
}  // namespace payments

class AutofillClient;
class AutofillDriver;

class TestLocalCardMigrationManager : public LocalCardMigrationManager {
 public:
  TestLocalCardMigrationManager(AutofillDriver* driver,
                                AutofillClient* client,
                                payments::TestPaymentsClient* payments_client,
                                TestPersonalDataManager* personal_data_manager);
  ~TestLocalCardMigrationManager() override;

  // Override the base function. Checks the existnece of billing customer number
  // and the experiment flag, but unlike the real class, does not check if the
  // user is signed in/syncing.
  bool IsCreditCardMigrationEnabled() override;

  // Returns whether the local card migration was triggered.
  bool LocalCardMigrationWasTriggered();

  // Returns whether the first round intermediate pop-up window was shown.
  bool IntermediatePromptWasShown();

  // Returns whether the main prompt window was shown.
  bool MainPromptWasShown();

  // Override the base function. When called, represents the intermediate prompt
  // is shown. Set the |intermediate_prompt_was_shown_|.
  void OnUserAcceptedIntermediateMigrationDialog() override;

  // Override the base function. When called, represent the main prompt is
  // shown. Set the |main_prompt_was_shown_|.
  void OnUserAcceptedMainMigrationDialog(
      const std::vector<std::string>& selected_cards) override;

  // Mock the Chrome Sync state in the LocalCardMigrationManager. If not set,
  // default to AutofillSyncSigninState::kSignedInAndSyncFeature.
  void ResetSyncState(AutofillSyncSigninState sync_state);

 private:
  void OnDidGetUploadDetails(
      bool is_from_settings_page,
      AutofillClient::PaymentsRpcResult result,
      const base::string16& context_token,
      std::unique_ptr<base::Value> legal_message,
      std::vector<std::pair<int, int>> supported_bin_ranges) override;

  bool local_card_migration_was_triggered_ = false;

  bool intermediate_prompt_was_shown_ = false;

  bool main_prompt_was_shown_ = false;

  TestPersonalDataManager* personal_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestLocalCardMigrationManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_LOCAL_CARD_MIGRATION_MANAGER_H_
