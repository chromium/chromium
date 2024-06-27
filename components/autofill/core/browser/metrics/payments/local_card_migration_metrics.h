// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_LOCAL_CARD_MIGRATION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_LOCAL_CARD_MIGRATION_METRICS_H_

#include "base/time/time.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

// Metrics to track events when local credit card migration is offered.
enum LocalCardMigrationBubbleOfferMetric {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The bubble is requested due to a credit card being used or
  // local card migration icon in the omnibox being clicked.
  LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED = 0,
  // The bubble is actually shown to the user.
  LOCAL_CARD_MIGRATION_BUBBLE_SHOWN = 1,
  NUM_LOCAL_CARD_MIGRATION_BUBBLE_OFFER_METRICS,
};

// Metrics to track user action result of the bubble when the bubble is
// closed.
enum LocalCardMigrationBubbleResultMetric {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The user explicitly accepted the offer.
  LOCAL_CARD_MIGRATION_BUBBLE_ACCEPTED = 0,
  // The user explicitly closed the bubble with the close button or ESC.
  LOCAL_CARD_MIGRATION_BUBBLE_CLOSED = 1,
  // The user did not interact with the bubble.
  LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED = 2,
  // The bubble lost its focus and was deactivated.
  LOCAL_CARD_MIGRATION_BUBBLE_LOST_FOCUS = 3,
  // The reason why the prompt is closed is not clear. Possible reason is the
  // logging function is invoked before the closed reason is correctly set.
  LOCAL_CARD_MIGRATION_BUBBLE_RESULT_UNKNOWN = 4,
  NUM_LOCAL_CARD_MIGRATION_BUBBLE_RESULT_METRICS,
};

// Metrics to record the decision on whether to offer local card migration.
enum class LocalCardMigrationDecisionMetric {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // All the required conditions are satisfied and main prompt is shown.
  OFFERED = 0,
  // Migration not offered because user uses new card.
  NOT_OFFERED_USE_NEW_CARD = 1,
  // Migration not offered because failed migration prerequisites.
  NOT_OFFERED_FAILED_PREREQUISITES = 2,
  // The Autofill StrikeDatabase decided not to allow offering migration
  // because max strike count was reached.
  NOT_OFFERED_REACHED_MAX_STRIKE_COUNT = 3,
  // Migration not offered because no migratable cards.
  NOT_OFFERED_NO_MIGRATABLE_CARDS = 4,
  // Met the migration requirements but the request to Payments for upload
  // details failed.
  NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED = 5,
  // Abandoned the migration because no supported local cards were left after
  // filtering out unsupported cards.
  NOT_OFFERED_NO_SUPPORTED_CARDS = 6,
  // User used a local card and they only have a single migratable local card
  // on file, we will offer Upstream instead.
  NOT_OFFERED_SINGLE_LOCAL_CARD = 7,
  // User used an unsupported local card, we will abort the migration.
  NOT_OFFERED_USE_UNSUPPORTED_LOCAL_CARD = 8,
  // Legal message was invalid, we will abort the migration.
  NOT_OFFERED_INVALID_LEGAL_MESSAGE = 9,
  kMaxValue = NOT_OFFERED_INVALID_LEGAL_MESSAGE,
};

// Metrics to track events when local card migration dialog is offered.
enum LocalCardMigrationDialogOfferMetric {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The dialog is shown to the user.
  LOCAL_CARD_MIGRATION_DIALOG_SHOWN = 0,
  // The dialog is not shown due to legal message being invalid.
  LOCAL_CARD_MIGRATION_DIALOG_NOT_SHOWN_INVALID_LEGAL_MESSAGE = 1,
  // The dialog is shown when migration feedback is available.
  LOCAL_CARD_MIGRATION_DIALOG_FEEDBACK_SHOWN = 2,
  // The dialog is shown when migration fails due to server error.
  LOCAL_CARD_MIGRATION_DIALOG_FEEDBACK_SERVER_ERROR_SHOWN = 3,
  NUM_LOCAL_CARD_MIGRATION_DIALOG_OFFER_METRICS,
};

// Metrics to track user interactions with the dialog.
enum LocalCardMigrationDialogUserInteractionMetric {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The user explicitly accepts the offer by clicking the save button.
  LOCAL_CARD_MIGRATION_DIALOG_CLOSED_SAVE_BUTTON_CLICKED = 0,
  // The user explicitly denies the offer by clicking the cancel button.
  LOCAL_CARD_MIGRATION_DIALOG_CLOSED_CANCEL_BUTTON_CLICKED = 1,
  // The user clicks the legal message.
  LOCAL_CARD_MIGRATION_DIALOG_LEGAL_MESSAGE_CLICKED = 2,
  // The user clicks the view card button after successfully migrated cards.
  LOCAL_CARD_MIGRATION_DIALOG_CLOSED_VIEW_CARDS_BUTTON_CLICKED = 3,
  // The user clicks the done button to close dialog after migration.
  LOCAL_CARD_MIGRATION_DIALOG_CLOSED_DONE_BUTTON_CLICKED = 4,
  // The user clicks the trash icon to delete invalid card.
  LOCAL_CARD_MIGRATION_DIALOG_DELETE_CARD_ICON_CLICKED = 5,
  NUM_LOCAL_CARD_MIGRATION_DIALOG_USER_INTERACTION_METRICS,
};

// These metrics are logged for each local card migration origin. These are
// used to derive the conversion rate for each triggering source.
enum LocalCardMigrationPromptMetric {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The intermediate bubble is shown to the user.
  INTERMEDIATE_BUBBLE_SHOWN = 0,
  // The intermediate bubble is accepted by the user.
  INTERMEDIATE_BUBBLE_ACCEPTED = 1,
  // The main dialog is shown to the user.
  MAIN_DIALOG_SHOWN = 2,
  // The main dialog is accepted by the user.
  MAIN_DIALOG_ACCEPTED = 3,
  NUM_LOCAL_CARD_MIGRATION_PROMPT_METRICS,
};

// Local card migration origin denotes from where the migration is triggered.
enum LocalCardMigrationOrigin {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Trigger when user submitted a form using local card.
  UseOfLocalCard,
  // Trigger when user submitted a form using server card.
  UseOfServerCard,
  // Trigger from settings page.
  SettingsPage,
};

void LogLocalCardMigrationBubbleOfferMetric(
    LocalCardMigrationBubbleOfferMetric metric,
    bool is_reshow);

void LogLocalCardMigrationBubbleResultMetric(
    LocalCardMigrationBubbleResultMetric metric,
    bool is_reshow);

void LogLocalCardMigrationDecisionMetric(
    LocalCardMigrationDecisionMetric metric);

void LogLocalCardMigrationDialogOfferMetric(
    LocalCardMigrationDialogOfferMetric metric);

void LogLocalCardMigrationDialogUserInteractionMetric(
    base::TimeDelta duration,
    LocalCardMigrationDialogUserInteractionMetric metric);

void LogLocalCardMigrationDialogUserSelectionPercentageMetric(int selected,
                                                              int total);

// When local card migration is not offered due to max strike limit reached,
// logs the occurrence.
void LogLocalCardMigrationNotOfferedDueToMaxStrikesMetric(
    AutofillMetrics::SaveTypeMetric metric);

void LogLocalCardMigrationPromptMetric(
    LocalCardMigrationOrigin local_card_migration_origin,
    LocalCardMigrationPromptMetric metric);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_LOCAL_CARD_MIGRATION_METRICS_H_
