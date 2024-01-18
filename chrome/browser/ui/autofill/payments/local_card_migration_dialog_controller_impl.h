// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_controller_observer.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/local_card_migration_dialog_controller.h"
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
  LocalCardMigrationDialogControllerImpl(
      const LocalCardMigrationDialogControllerImpl&) = delete;
  LocalCardMigrationDialogControllerImpl& operator=(
      const LocalCardMigrationDialogControllerImpl&) = delete;
  ~LocalCardMigrationDialogControllerImpl() override;

  void ShowOfferDialog(
      const LegalMessageLines& legal_message_lines,
      const std::string& user_email,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      payments::PaymentsAutofillClient::LocalCardMigrationCallback
          start_migrating_cards_callback);

  // When migration is finished, update the credit card icon. Also passes
  // |tip_message|, and |migratable_credit_cards| to controller.
  void UpdateCreditCardIcon(
      const std::u16string& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      payments::PaymentsAutofillClient::MigrationDeleteCardCallback
          delete_local_card_callback);

  // If the user clicks on the credit card icon in the omnibox, we show the
  // feedback dialog containing the uploading results of the cards that the
  // user selected to upload.
  void ShowFeedbackDialog();

  // If the user clicks on the credit card icon in the omnibox after the
  // migration request failed due to some internal server errors, we show the
  // error dialog containing an error message.
  void ShowErrorDialog();

  void AddObserver(LocalCardMigrationControllerObserver* observer);

  // LocalCardMigrationDialogController:
  LocalCardMigrationDialogState GetViewState() const override;
  const std::vector<MigratableCreditCard>& GetCardList() const override;
  const LegalMessageLines& GetLegalMessageLines() const override;
  const std::u16string& GetTipMessage() const override;
  const std::string& GetUserEmail() const override;
  void OnSaveButtonClicked(
      const std::vector<std::string>& selected_cards_guids) override;
  void OnCancelButtonClicked() override;
  void OnDoneButtonClicked() override;
  void OnViewCardsButtonClicked() override;
  void OnLegalMessageLinkClicked(const GURL& url) override;
  void DeleteCard(const std::string& deleted_card_guid) override;
  void OnDialogClosed() override;
  bool AllCardsInvalid() const override;

  // Returns nullptr if no dialog is currently shown.
  LocalCardMigrationDialog* local_card_migration_dialog_view() const;

 protected:
  explicit LocalCardMigrationDialogControllerImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      LocalCardMigrationDialogControllerImpl>;

  void OpenUrl(const GURL& url);

  void UpdateLocalCardMigrationIcon();

  // The dialog is showing cards of which the migration failed. We will show
  // the "Almost done" dialog in this case.
  bool HasFailedCard() const;

  void NotifyMigrationNoLongerAvailable();
  void NotifyMigrationStarted();

  raw_ptr<LocalCardMigrationDialog> local_card_migration_dialog_ = nullptr;

  raw_ptr<PrefService> pref_service_;

  LocalCardMigrationDialogState view_state_;

  LegalMessageLines legal_message_lines_;

  // Invoked when the save button is clicked. Will return a vector containing
  // GUIDs of cards that the user selected to upload.
  payments::PaymentsAutofillClient::LocalCardMigrationCallback
      start_migrating_cards_callback_;

  // Invoked when the trash can button in the action-requied dialog is clicked.
  // Will pass a string of GUID of the card the user selected to delete from
  // local storage to LocalCardMigrationManager.
  payments::PaymentsAutofillClient::MigrationDeleteCardCallback
      delete_local_card_callback_;

  // Local copy of the MigratableCreditCards vector passed from
  // LocalCardMigrationManager. Used in constructing the
  // LocalCardMigrationDialogView.
  std::vector<MigratableCreditCard> migratable_credit_cards_;

  // Timer used to measure the amount of time that the local card migration
  // dialog is visible to users.
  base::ElapsedTimer dialog_is_visible_duration_timer_;

  // The message containing information from Google Payments. Shown in the
  // feedback dialogs after migration process is finished.
  std::u16string tip_message_;

  // The user email shown in the dialogs.
  std::string user_email_;

  // Contains observer listening to user's interactions with the dialog. The
  // observer is responsible for setting flow step upon these interactions.
  base::ObserverList<LocalCardMigrationControllerObserver>::Unchecked
      observer_list_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_
