// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/manual_fallback_metrics.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
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
  EmitExplicitlyTriggeredMetric(address_context_menu_state_, "Address");
  EmitExplicitlyTriggeredMetric(credit_card_context_menu_state_, "CreditCard");
  EmitFillAfterSuggestionMetric(address_suggestions_state_, "Address");
  EmitFillAfterSuggestionMetric(credit_card_suggestions_state_, "CreditCard");
}

void ManualFallbackEventLogger::OnDidShowSuggestions(
    FillingProduct target_filling_product) {
  UpdateSuggestionStateForFillingProduct(target_filling_product,
                                         SuggestionState::kShown);
}

void ManualFallbackEventLogger::OnDidFillSuggestion(
    FillingProduct target_filling_product) {
  UpdateSuggestionStateForFillingProduct(target_filling_product,
                                         SuggestionState::kFilled);
}

void ManualFallbackEventLogger::ContextMenuEntryShown(
    FillingProduct target_filling_product) {
  switch (target_filling_product) {
    case FillingProduct::kAddress:
      UpdateContextMenuEntryState(ContextMenuEntryState::kShown,
                                  address_context_menu_state_);
      break;
    case FillingProduct::kCreditCard:
      UpdateContextMenuEntryState(ContextMenuEntryState::kShown,
                                  credit_card_context_menu_state_);
      break;
    case FillingProduct::kNone:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIban:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kPassword:
    case FillingProduct::kCompose:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kPredictionImprovements:
    case FillingProduct::kStandaloneCvc:
      NOTREACHED();
  }
}

void ManualFallbackEventLogger::ContextMenuEntryAccepted(
    FillingProduct target_filling_product) {
  switch (target_filling_product) {
    case FillingProduct::kAddress:
      UpdateContextMenuEntryState(ContextMenuEntryState::kAccepted,
                                  address_context_menu_state_);
      break;
    case FillingProduct::kCreditCard:
      UpdateContextMenuEntryState(ContextMenuEntryState::kAccepted,
                                  credit_card_context_menu_state_);
      break;
    case FillingProduct::kNone:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIban:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kPassword:
    case FillingProduct::kCompose:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kPredictionImprovements:
    case FillingProduct::kStandaloneCvc:
      NOTREACHED();
  }
}

// static
void ManualFallbackEventLogger::UpdateContextMenuEntryState(
    ContextMenuEntryState new_state,
    ContextMenuEntryState& old_state) {
  switch (new_state) {
    case ContextMenuEntryState::kNotShown:
      NOTREACHED();
    case ContextMenuEntryState::kShown:
      if (old_state != ContextMenuEntryState::kAccepted) {
        old_state = new_state;
      }
      break;
    case ContextMenuEntryState::kAccepted:
      CHECK_NE(old_state, ContextMenuEntryState::kNotShown);
      old_state = new_state;
      break;
  }
}

void ManualFallbackEventLogger::UpdateSuggestionStateForFillingProduct(
    FillingProduct filling_product,
    SuggestionState new_state) {
  // This lambda acts similar to `UpdateContextMenuEntryState()`, but trying to
  // change the `SuggestionState` instead of the `ContextMenuEntryState`.
  const auto update_suggestion_state = [](SuggestionState new_state,
                                          SuggestionState& old_state) {
    switch (new_state) {
      case SuggestionState::kNotShown:
        NOTREACHED();
      case SuggestionState::kShown:
        if (old_state != SuggestionState::kFilled) {
          old_state = new_state;
        }
        break;
      case SuggestionState::kFilled:
        CHECK_NE(old_state, SuggestionState::kNotShown);
        old_state = new_state;
        break;
    }
  };

  switch (filling_product) {
    case FillingProduct::kAddress:
      update_suggestion_state(new_state, address_suggestions_state_);
      break;
    case FillingProduct::kCreditCard:
      update_suggestion_state(new_state, credit_card_suggestions_state_);
      break;
    case FillingProduct::kNone:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIban:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kPassword:
    case FillingProduct::kCompose:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kPredictionImprovements:
    case FillingProduct::kStandaloneCvc:
      NOTREACHED();
  }
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
