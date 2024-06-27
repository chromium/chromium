// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/local_card_migration_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace autofill::autofill_metrics {

void LogLocalCardMigrationBubbleOfferMetric(
    LocalCardMigrationBubbleOfferMetric metric,
    bool is_reshow) {
  DCHECK_LT(metric, NUM_LOCAL_CARD_MIGRATION_BUBBLE_OFFER_METRICS);
  std::string histogram_name = "Autofill.LocalCardMigrationBubbleOffer.";
  histogram_name += is_reshow ? "Reshows" : "FirstShow";
  base::UmaHistogramEnumeration(histogram_name, metric,
                                NUM_LOCAL_CARD_MIGRATION_BUBBLE_OFFER_METRICS);
}

void LogLocalCardMigrationBubbleResultMetric(
    LocalCardMigrationBubbleResultMetric metric,
    bool is_reshow) {
  DCHECK_LT(metric, NUM_LOCAL_CARD_MIGRATION_BUBBLE_RESULT_METRICS);
  std::string suffix = is_reshow ? ".Reshows" : ".FirstShow";
  base::UmaHistogramEnumeration(
      "Autofill.LocalCardMigrationBubbleResult" + suffix, metric,
      NUM_LOCAL_CARD_MIGRATION_BUBBLE_RESULT_METRICS);
}

void LogLocalCardMigrationDecisionMetric(
    LocalCardMigrationDecisionMetric metric) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.LocalCardMigrationDecision", metric);
}

void LogLocalCardMigrationDialogOfferMetric(
    LocalCardMigrationDialogOfferMetric metric) {
  DCHECK_LT(metric, NUM_LOCAL_CARD_MIGRATION_DIALOG_OFFER_METRICS);
  std::string histogram_name = "Autofill.LocalCardMigrationDialogOffer";
  base::UmaHistogramEnumeration(histogram_name, metric,
                                NUM_LOCAL_CARD_MIGRATION_DIALOG_OFFER_METRICS);
}

void LogLocalCardMigrationDialogUserInteractionMetric(
    base::TimeDelta duration,
    LocalCardMigrationDialogUserInteractionMetric metric) {
  DCHECK_LT(metric, NUM_LOCAL_CARD_MIGRATION_DIALOG_USER_INTERACTION_METRICS);
  base::UmaHistogramEnumeration(
      "Autofill.LocalCardMigrationDialogUserInteraction", metric,
      NUM_LOCAL_CARD_MIGRATION_DIALOG_USER_INTERACTION_METRICS);

  // Do not log duration metrics for
  // LOCAL_CARD_MIGRATION_DIALOG_DELETE_CARD_ICON_CLICKED, as it can happen
  // multiple times in one dialog.
  std::string suffix;
  switch (metric) {
    case LOCAL_CARD_MIGRATION_DIALOG_CLOSED_SAVE_BUTTON_CLICKED:
      suffix = "Accepted";
      break;
    case LOCAL_CARD_MIGRATION_DIALOG_CLOSED_CANCEL_BUTTON_CLICKED:
      suffix = "Denied";
      break;
    case LOCAL_CARD_MIGRATION_DIALOG_CLOSED_VIEW_CARDS_BUTTON_CLICKED:
    case LOCAL_CARD_MIGRATION_DIALOG_CLOSED_DONE_BUTTON_CLICKED:
      suffix = "Closed";
      break;
    default:
      return;
  }

  base::UmaHistogramLongTimes(
      "Autofill.LocalCardMigrationDialogActiveDuration." + suffix, duration);
}

void LogLocalCardMigrationDialogUserSelectionPercentageMetric(int selected,
                                                              int total) {
  UMA_HISTOGRAM_PERCENTAGE(
      "Autofill.LocalCardMigrationDialogUserSelectionPercentage",
      100 * selected / total);
}

void LogLocalCardMigrationNotOfferedDueToMaxStrikesMetric(
    AutofillMetrics::SaveTypeMetric metric) {
  UMA_HISTOGRAM_ENUMERATION(
      "Autofill.StrikeDatabase.LocalCardMigrationNotOfferedDueToMaxStrikes",
      metric);
}

void LogLocalCardMigrationPromptMetric(
    LocalCardMigrationOrigin local_card_migration_origin,
    LocalCardMigrationPromptMetric metric) {
  DCHECK_LT(metric, NUM_LOCAL_CARD_MIGRATION_PROMPT_METRICS);
  std::string histogram_name = "Autofill.LocalCardMigrationOrigin.";
  // Switch to different sub-histogram depending on local card migration origin.
  switch (local_card_migration_origin) {
    case LocalCardMigrationOrigin::UseOfLocalCard:
      histogram_name += "UseOfLocalCard";
      break;
    case LocalCardMigrationOrigin::UseOfServerCard:
      histogram_name += "UseOfServerCard";
      break;
    case LocalCardMigrationOrigin::SettingsPage:
      histogram_name += "SettingsPage";
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  base::UmaHistogramEnumeration(histogram_name, metric,
                                NUM_LOCAL_CARD_MIGRATION_PROMPT_METRICS);
}

}  // namespace autofill::autofill_metrics
