// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/fallback_autocomplete_unrecognized_metrics.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace autofill::autofill_metrics {

AutocompleteUnrecognizedFallbackEventLogger::
    ~AutocompleteUnrecognizedFallbackEventLogger() {
  // Emit the explicit triggering metric for recognized and unrecognized fields.
  EmitExplicitlyTriggeredMetric(ac_unrecognized_context_menu_state_,
                                "ClassifiedFieldAutocompleteUnrecognized");
  EmitExplicitlyTriggeredMetric(ac_recognized_context_menu_state_,
                                "ClassifiedFieldAutocompleteRecognized");
  EmitFillAfterSuggestionMetric();
}

void AutocompleteUnrecognizedFallbackEventLogger::OnDidShowSuggestions() {
  if (suggestion_state_ == SuggestionState::kNotShown) {
    suggestion_state_ = SuggestionState::kShown;
  }
}

void AutocompleteUnrecognizedFallbackEventLogger::
    OnDidFillFormFillingSuggestion() {
  // Since the `AutocompleteUnrecognizedFallbackEventLogger` is only notified
  // about autocomplete=unrecognized fields, it is possible that to reach
  // `OnDidFillFormFillingSuggestion()` with `suggestion_state_` kNotShown. This
  // happens when the website dynamically changes the autocomplete attribute to
  // unrecognized after triggering (regular) suggestions.
  if (suggestion_state_ == SuggestionState::kShown) {
    suggestion_state_ = SuggestionState::kFilled;
  }
}

void AutocompleteUnrecognizedFallbackEventLogger::ContextMenuEntryShown(
    bool address_field_has_ac_unrecognized) {
  ContextMenuEntryState& state = address_field_has_ac_unrecognized
                                     ? ac_unrecognized_context_menu_state_
                                     : ac_recognized_context_menu_state_;
  if (state != ContextMenuEntryState::kAccepted) {
    state = ContextMenuEntryState::kShown;
  }
}

void AutocompleteUnrecognizedFallbackEventLogger::ContextMenuEntryAccepted(
    bool address_field_has_ac_unrecognized) {
  ContextMenuEntryState& state = address_field_has_ac_unrecognized
                                     ? ac_unrecognized_context_menu_state_
                                     : ac_recognized_context_menu_state_;
  CHECK_NE(state, ContextMenuEntryState::kNotShown);
  state = ContextMenuEntryState::kAccepted;
}

void AutocompleteUnrecognizedFallbackEventLogger::EmitExplicitlyTriggeredMetric(
    ContextMenuEntryState state,
    std::string_view bucket) {
  if (state == ContextMenuEntryState::kNotShown) {
    return;
  }

  auto metric_name = [](std::string_view token) {
    return base::StrCat(
        {"Autofill.ManualFallback.ExplicitlyTriggered.", token, ".Address"});
  };
  // Emit to the bucket corresponding to the `state` and to the "Total" bucket.
  const bool was_accepted = state == ContextMenuEntryState::kAccepted;
  base::UmaHistogramBoolean(metric_name(bucket), was_accepted);
  base::UmaHistogramBoolean(metric_name("Total"), was_accepted);
}

void AutocompleteUnrecognizedFallbackEventLogger::
    EmitFillAfterSuggestionMetric() {
  if (suggestion_state_ == SuggestionState::kNotShown) {
    return;
  }
  base::UmaHistogramBoolean(
      "Autofill.Funnel.ClassifiedFieldAutocompleteUnrecognized."
      "FillAfterSuggestion.Address",
      suggestion_state_ == SuggestionState::kFilled);
}

}  // namespace autofill::autofill_metrics
