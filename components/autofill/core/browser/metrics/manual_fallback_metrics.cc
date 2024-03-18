// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/manual_fallback_metrics.h"
#include "components/autofill/core/browser/filling_product.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace autofill::autofill_metrics {

void LogAddNewAddressPromptOutcome(AutofillAddNewAddressPromptOutcome outcome) {
  base::UmaHistogramEnumeration(
      "Autofill.ManualFallback.AddNewAddressPromptShown", outcome);
}

ManualFallbackEventLogger::~ManualFallbackEventLogger() {
  // Emit the explicit triggering metric for fields that were either
  // unclassified or classified as something differently from the targeted
  // `FillingProduct`.
  EmitExplicitlyTriggeredMetric(not_classified_as_target_filling_address,
                                "Address");
  EmitExplicitlyTriggeredMetric(not_classified_as_target_filling_credit_card,
                                "CreditCard");
  EmitFillAfterSuggestionMetric(address_suggestions_state_, "Address");
  EmitFillAfterSuggestionMetric(credit_card_suggestions_state_, "CreditCard");
}

void ManualFallbackEventLogger::OnDidShowSuggestions(
    FillingProduct target_filling_product) {
  CHECK(target_filling_product == FillingProduct::kAddress ||
        target_filling_product == FillingProduct::kCreditCard);
  SuggestionState& suggestion_state =
      target_filling_product == FillingProduct::kAddress
          ? address_suggestions_state_
          : credit_card_suggestions_state_;

  if (suggestion_state == SuggestionState::kNotShown) {
    suggestion_state = SuggestionState::kShown;
  }
}

void ManualFallbackEventLogger::OnDidFillSuggestion(
    FillingProduct target_filling_product) {
  CHECK(target_filling_product == FillingProduct::kAddress ||
        target_filling_product == FillingProduct::kCreditCard);
  SuggestionState& suggestion_state =
      target_filling_product == FillingProduct::kAddress
          ? address_suggestions_state_
          : credit_card_suggestions_state_;

  if (suggestion_state == SuggestionState::kShown) {
    suggestion_state = SuggestionState::kFilled;
  }
}

void ManualFallbackEventLogger::ContextMenuEntryShown(
    bool address_fallback_present,
    bool payments_fallback_present) {
  if (address_fallback_present && not_classified_as_target_filling_address !=
                                      ContextMenuEntryState::kAccepted) {
    not_classified_as_target_filling_address = ContextMenuEntryState::kShown;
  }
  if (payments_fallback_present &&
      not_classified_as_target_filling_credit_card !=
          ContextMenuEntryState::kAccepted) {
    not_classified_as_target_filling_credit_card =
        ContextMenuEntryState::kShown;
  }
}

void ManualFallbackEventLogger::ContextMenuEntryAccepted(
    FillingProduct target_filling_product) {
  CHECK(target_filling_product == FillingProduct::kAddress ||
        target_filling_product == FillingProduct::kCreditCard);
  ContextMenuEntryState& state =
      target_filling_product == FillingProduct::kAddress
          ? not_classified_as_target_filling_address
          : not_classified_as_target_filling_credit_card;
  CHECK_NE(state, ContextMenuEntryState::kNotShown);

  state = ContextMenuEntryState::kAccepted;
}

void ManualFallbackEventLogger::EmitExplicitlyTriggeredMetric(
    ContextMenuEntryState state,
    std::string_view bucket) {
  if (state == ContextMenuEntryState::kNotShown) {
    return;
  }

  auto metric_name = [](std::string_view token) {
    return base::StrCat(
        {"Autofill.ManualFallback.ExplicitlyTriggered."
         "NotClassifiedAsTargetFilling.",
         token});
  };
  // Emit to the bucket corresponding to the `state` and to the "Total" bucket.
  const bool was_accepted = state == ContextMenuEntryState::kAccepted;
  base::UmaHistogramBoolean(metric_name(bucket), was_accepted);
  base::UmaHistogramBoolean(metric_name("Total"), was_accepted);
}

void ManualFallbackEventLogger::EmitFillAfterSuggestionMetric(
    SuggestionState suggestion_state,
    std::string_view bucket) {
  if (suggestion_state == SuggestionState::kNotShown) {
    return;
  }
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.Funnel.NotClassifiedAsTargetFilling."
                    "FillAfterSuggestion.",
                    bucket}),
      suggestion_state == SuggestionState::kFilled);
}

}  // namespace autofill::autofill_metrics
