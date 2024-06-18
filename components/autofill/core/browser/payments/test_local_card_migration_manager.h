// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_LOCAL_CARD_MIGRATION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_LOCAL_CARD_MIGRATION_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/autofill/core/browser/payments/local_card_migration_manager.h"

namespace autofill {

class TestLocalCardMigrationManager : public LocalCardMigrationManager {
 public:
  using LocalCardMigrationManager::LocalCardMigrationManager;

  TestLocalCardMigrationManager(const TestLocalCardMigrationManager&) = delete;
  TestLocalCardMigrationManager& operator=(
      const TestLocalCardMigrationManager&) = delete;

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

  // By default, the `LocalCardMigrationManager` syncing state is "signed in
  // and sync-the-feature enabled". Using this function, tests can simulate
  // sync transport mode.
  void EnablePaymentsWalletSyncInTransportMode();

 private:
  void OnDidGetUploadDetails(
      bool is_from_settings_page,
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const std::u16string& context_token,
      std::unique_ptr<base::Value::Dict> legal_message,
      std::vector<std::pair<int, int>> supported_bin_ranges) override;

  bool local_card_migration_was_triggered_ = false;

  bool intermediate_prompt_was_shown_ = false;

  bool main_prompt_was_shown_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_TEST_LOCAL_CARD_MIGRATION_MANAGER_H_
