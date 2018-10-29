// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/ui/local_card_migration_dialog_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class LocalCardMigrationDialog;

// This per-tab controller is lazily initialized and owns a
// LocalCardMigrationDialog. It's also responsible for reshowing the original
// dialog that the migration dialog interrupted.
class LocalCardMigrationDialogControllerImpl
    : public LocalCardMigrationDialogController,
      public content::WebContentsUserData<
          LocalCardMigrationDialogControllerImpl> {
 public:
  ~LocalCardMigrationDialogControllerImpl() override;

  void ShowDialog(
      std::unique_ptr<base::DictionaryValue> legal_message,
      LocalCardMigrationDialog* local_card_migration_dialog,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      AutofillClient::LocalCardMigrationCallback
          start_migrating_cards_callback);

  // LocalCardMigrationDialogController:
  LocalCardMigrationDialogState GetViewState() const override;
  const std::vector<MigratableCreditCard>& GetCardList() const override;
  const LegalMessageLines& GetLegalMessageLines() const override;
  void OnSaveButtonClicked(
      const std::vector<std::string>& selected_cards_guids) override;
  void OnCancelButtonClicked() override;
  void OnViewCardsButtonClicked() override;
  void OnLegalMessageLinkClicked(const GURL& url) override;
  void OnDialogClosed() override;

 protected:
  explicit LocalCardMigrationDialogControllerImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      LocalCardMigrationDialogControllerImpl>;

  void OpenUrl(const GURL& url);

  content::WebContents* web_contents_;

  LocalCardMigrationDialog* local_card_migration_dialog_;

  PrefService* pref_service_;

  LocalCardMigrationDialogState view_state_;

  LegalMessageLines legal_message_lines_;

  // Invoked when the save button is clicked. Will return a vector containing
  // GUIDs of cards that the user selected to upload.
  AutofillClient::LocalCardMigrationCallback start_migrating_cards_callback_;

  // Local copy of the MigratableCreditCards vector passed from
  // LocalCardMigrationManager. Used in constructing the
  // LocalCardMigrationDialogView.
  std::vector<MigratableCreditCard> migratable_credit_cards_;

  // Timer used to measure the amount of time that the local card migration
  // dialog is visible to users.
  base::ElapsedTimer dialog_is_visible_duration_timer_;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationDialogControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_
