// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANAGE_MIGRATION_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANAGE_MIGRATION_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_controller_observer.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_dialog_controller_impl.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Possible steps the migration flow could be in.
enum class LocalCardMigrationFlowStep {
  // Migration flow step unknown.
  UNKNOWN,
  // No migration flow bubble or dialog is shown.
  NOT_SHOWN,
  // Should show the bubble that offers users to continue with the migration
  // flow.
  PROMO_BUBBLE,
  // Should show the dialog that offers users to migrate credit cards to
  // Payments server.
  OFFER_DIALOG,
  // Migration is in process and result is pending after users click the save
  // button.
  // Should show credit card icon and the animation.
  MIGRATION_RESULT_PENDING,
  // Migration is finished. Should show the credit card icon when migration
  // is finished and the feedback dialog is ready.
  MIGRATION_FINISHED,
  // Migration is finished and there are some cards save fails, or payments
  // server error.
  MIGRATION_FAILED,
  // Should show the feedback dialog containing the migration results of cards
  // that the user selected to upload after the user clicking the credit card
  // icon.
  FEEDBACK_DIALOG,
  // Should show the error dialog if the Payments Rpc request failed after the
  // user clicks the credit card icon.
  ERROR_DIALOG,
};

// Controller controls the step of migration flow and is responsible
// for interacting with LocalCardMigrationIconView.
class ManageMigrationUiController
    : public LocalCardMigrationControllerObserver,
      public content::WebContentsUserData<ManageMigrationUiController> {
 public:
  ManageMigrationUiController(const ManageMigrationUiController&) = delete;
  ManageMigrationUiController& operator=(const ManageMigrationUiController&) =
      delete;
  ~ManageMigrationUiController() override;

  void ShowBubble(base::OnceClosure show_migration_dialog_closure);

  void ShowOfferDialog(
      const LegalMessageLines& legal_message_lines,
      const std::string& user_email,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      payments::PaymentsAutofillClient::LocalCardMigrationCallback
          start_migrating_cards_callback);

  void UpdateCreditCardIcon(
      const bool has_server_error,
      const std::u16string& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      payments::PaymentsAutofillClient::MigrationDeleteCardCallback
          delete_local_card_callback);

  void OnUserClickedCreditCardIcon();

  LocalCardMigrationFlowStep GetFlowStep() const;

  bool IsIconVisible() const;

  // Returns the bubble or the dialog view, respectively.
  AutofillBubbleBase* GetBubbleView();
  LocalCardMigrationDialog* GetDialogView();

  // LocalCardMigrationControllerObserver:
  void OnMigrationNoLongerAvailable() override;
  void OnMigrationStarted() override;

 protected:
  explicit ManageMigrationUiController(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<ManageMigrationUiController>;

  // Gets the card migration bubble controller for this `WebContents`.
  LocalCardMigrationBubbleControllerImpl* GetBubbleController();
  // Gets the card migration dialog controller for this `WebContents`.
  LocalCardMigrationDialogControllerImpl* GetDialogController();

  void ReshowBubble();

  void ShowErrorDialog();

  void ShowFeedbackDialog();

  // This indicates what step the migration flow is currently in and
  // what should be shown next.
  LocalCardMigrationFlowStep flow_step_ = LocalCardMigrationFlowStep::NOT_SHOWN;

  // This indicates if we should show error dialog or normal feedback dialog
  // after users click the credit card icon.
  bool show_error_dialog_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_MANAGE_MIGRATION_UI_CONTROLLER_H_
