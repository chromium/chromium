// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_dialog.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"

namespace autofill {

ManageMigrationUiController::ManageMigrationUiController(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ManageMigrationUiController>(*web_contents) {
  // TODO(crbug.com/40258491): Use `ScopedObservation` once the observer has a
  // `OnDestroying` method to avoid that the source has dangling references to
  // `this`.
  LocalCardMigrationBubbleControllerImpl::CreateForWebContents(web_contents);
  GetBubbleController()->AddObserver(this);

  LocalCardMigrationDialogControllerImpl::CreateForWebContents(web_contents);
  GetDialogController()->AddObserver(this);
}

ManageMigrationUiController::~ManageMigrationUiController() = default;

LocalCardMigrationBubbleControllerImpl*
ManageMigrationUiController::GetBubbleController() {
  return LocalCardMigrationBubbleControllerImpl::FromWebContents(
      &GetWebContents());
}

LocalCardMigrationDialogControllerImpl*
ManageMigrationUiController::GetDialogController() {
  return LocalCardMigrationDialogControllerImpl::FromWebContents(
      &GetWebContents());
}

void ManageMigrationUiController::ShowBubble(
    base::OnceClosure show_migration_dialog_closure) {
  flow_step_ = LocalCardMigrationFlowStep::PROMO_BUBBLE;
  DCHECK(GetBubbleController());
  GetBubbleController()->ShowBubble(std::move(show_migration_dialog_closure));
}

void ManageMigrationUiController::ShowOfferDialog(
    const LegalMessageLines& legal_message_lines,
    const std::string& user_email,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    payments::PaymentsAutofillClient::LocalCardMigrationCallback
        start_migrating_cards_callback) {
  flow_step_ = LocalCardMigrationFlowStep::OFFER_DIALOG;
  DCHECK(GetDialogController());
  GetDialogController()->ShowOfferDialog(
      legal_message_lines, user_email, migratable_credit_cards,
      std::move(start_migrating_cards_callback));
}

void ManageMigrationUiController::UpdateCreditCardIcon(
    const bool has_server_error,
    const std::u16string& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    payments::PaymentsAutofillClient::MigrationDeleteCardCallback
        delete_local_card_callback) {
  if (!GetDialogController()) {
    return;
  }

  DCHECK_EQ(flow_step_, LocalCardMigrationFlowStep::MIGRATION_RESULT_PENDING);
  flow_step_ = LocalCardMigrationFlowStep::MIGRATION_FINISHED;
  for (const auto& cc : migratable_credit_cards) {
    if (cc.migration_status() ==
        MigratableCreditCard::MigrationStatus::FAILURE_ON_UPLOAD) {
      flow_step_ = LocalCardMigrationFlowStep::MIGRATION_FAILED;
      break;
    }
  }
  if (has_server_error)
    flow_step_ = LocalCardMigrationFlowStep::MIGRATION_FAILED;

  // Show error dialog when |has_server_error| is true, which indicates
  // Payments Rpc failure.
  show_error_dialog_ = has_server_error;

  GetDialogController()->UpdateCreditCardIcon(
      tip_message, migratable_credit_cards, delete_local_card_callback);
}

void ManageMigrationUiController::OnUserClickedCreditCardIcon() {
  switch (flow_step_) {
    case LocalCardMigrationFlowStep::PROMO_BUBBLE: {
      ReshowBubble();
      break;
    }
    case LocalCardMigrationFlowStep::MIGRATION_FINISHED: {
      ShowFeedbackDialog();
      break;
    }
    case LocalCardMigrationFlowStep::MIGRATION_FAILED: {
      show_error_dialog_ ? ShowErrorDialog() : ShowFeedbackDialog();
      break;
    }
    default: {
      break;
    }
  }
}

LocalCardMigrationFlowStep ManageMigrationUiController::GetFlowStep() const {
  return flow_step_;
}

bool ManageMigrationUiController::IsIconVisible() const {
  DCHECK_NE(flow_step_, LocalCardMigrationFlowStep::UNKNOWN);
  return flow_step_ != LocalCardMigrationFlowStep::NOT_SHOWN;
}

AutofillBubbleBase* ManageMigrationUiController::GetBubbleView() {
  LocalCardMigrationBubbleControllerImpl* controller = GetBubbleController();
  return controller ? controller->local_card_migration_bubble_view() : nullptr;
}

LocalCardMigrationDialog* ManageMigrationUiController::GetDialogView() {
  LocalCardMigrationDialogControllerImpl* controller = GetDialogController();
  return controller ? controller->local_card_migration_dialog_view() : nullptr;
}

void ManageMigrationUiController::OnMigrationNoLongerAvailable() {
  flow_step_ = LocalCardMigrationFlowStep::NOT_SHOWN;
}

void ManageMigrationUiController::OnMigrationStarted() {
  flow_step_ = LocalCardMigrationFlowStep::MIGRATION_RESULT_PENDING;
}

void ManageMigrationUiController::ReshowBubble() {
  if (!GetBubbleController()) {
    return;
  }

  DCHECK_EQ(flow_step_, LocalCardMigrationFlowStep::PROMO_BUBBLE);
  GetBubbleController()->ReshowBubble();
}

void ManageMigrationUiController::ShowErrorDialog() {
  if (!GetDialogController()) {
    return;
  }

  DCHECK_EQ(flow_step_, LocalCardMigrationFlowStep::MIGRATION_FINISHED);
  flow_step_ = LocalCardMigrationFlowStep::ERROR_DIALOG;
  GetDialogController()->ShowErrorDialog();
}

void ManageMigrationUiController::ShowFeedbackDialog() {
  if (!GetDialogController()) {
    return;
  }

  DCHECK(flow_step_ == LocalCardMigrationFlowStep::MIGRATION_FINISHED ||
         flow_step_ == LocalCardMigrationFlowStep::MIGRATION_FAILED);
  flow_step_ = LocalCardMigrationFlowStep::FEEDBACK_DIALOG;
  GetDialogController()->ShowFeedbackDialog();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ManageMigrationUiController);

}  // namespace autofill
