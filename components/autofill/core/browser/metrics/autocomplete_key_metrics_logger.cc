// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autocomplete_key_metrics_logger.h"

#include <algorithm>
#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_util.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table_label_sensitive.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill::autofill_metrics {

namespace {

std::string_view MatchingTypeToSuffix(MatchingType matching_type) {
  switch (matching_type) {
    case MatchingType::kLabel:
      return ".Label";
    case MatchingType::kName:
      return ".Name";
    case MatchingType::kNameAndLabel:
      return ".BothNameAndLabel";
    case MatchingType::kUnknown:
      NOTREACHED();
  }
}

std::optional<MatchingType> ExtractMatchingType(const Suggestion& suggestion) {
  if (const auto* entry = std::get_if<AutocompleteSearchResultLabelSensitive>(
          &suggestion.payload)) {
    if (entry->matching_type() != MatchingType::kUnknown) {
      return entry->matching_type();
    }
  }
  return std::nullopt;
}

}  // namespace

AutocompleteKeyMetricsLogger::FieldState::FieldState() = default;
AutocompleteKeyMetricsLogger::FieldState::FieldState(const FieldState&) =
    default;
AutocompleteKeyMetricsLogger::FieldState&
AutocompleteKeyMetricsLogger::FieldState::operator=(const FieldState&) =
    default;
AutocompleteKeyMetricsLogger::FieldState::FieldState(FieldState&&) = default;
AutocompleteKeyMetricsLogger::FieldState&
AutocompleteKeyMetricsLogger::FieldState::operator=(FieldState&&) = default;
AutocompleteKeyMetricsLogger::FieldState::~FieldState() = default;

AutocompleteKeyMetricsLogger::AutocompleteKeyMetricsLogger() = default;
AutocompleteKeyMetricsLogger::~AutocompleteKeyMetricsLogger() = default;

void AutocompleteKeyMetricsLogger::OnSuggestionsShown(
    FieldGlobalId field_id,
    base::span<const Suggestion> suggestions) {
  if (!std::ranges::contains(suggestions, SuggestionType::kAutocompleteEntry,
                             &Suggestion::type)) {
    return;
  }

  FieldState& state = field_states_[field_id];
  state.any_suggestion_shown = true;
  if (base::FeatureList::IsEnabled(
          features::kAutofillLabelSensitiveAutocomplete)) {
    for (const Suggestion& suggestion : suggestions) {
      if (suggestion.type != SuggestionType::kAutocompleteEntry) {
        continue;
      }
      if (std::optional<MatchingType> matching_type =
              ExtractMatchingType(suggestion)) {
        state.shown_matching_types.insert(*matching_type);
      }
    }
  }
}

void AutocompleteKeyMetricsLogger::OnSuggestionSelected(
    FieldGlobalId field_id,
    const Suggestion& suggestion) {
  if (suggestion.type != SuggestionType::kAutocompleteEntry) {
    return;
  }
  FieldState& state = field_states_[field_id];
  state.filled_value = suggestion.main_text.value;
  if (base::FeatureList::IsEnabled(
          features::kAutofillLabelSensitiveAutocomplete)) {
    state.accepted_matching_type = ExtractMatchingType(suggestion);
  }
}

void AutocompleteKeyMetricsLogger::OnFormSubmitted(
    const FormData& form,
    const FormStructure* form_structure) {
  const bool is_label_sensitive_enabled = base::FeatureList::IsEnabled(
      features::kAutofillLabelSensitiveAutocomplete);
  for (const FormFieldData& field : form.fields()) {
    auto node = field_states_.extract(field.global_id());
    if (!node) {
      continue;
    }
    const FieldState& state = node.mapped();
    const bool any_suggestion_accepted = state.filled_value.has_value();

    if (state.any_suggestion_shown) {
      base::UmaHistogramBoolean("Autocomplete.KeyMetrics.FillingAcceptance",
                                any_suggestion_accepted);
      if (const AutofillField* autofill_field =
              form_structure ? form_structure->GetFieldById(field.global_id())
                             : nullptr) {
        for (FieldType field_type : autofill_field->Type().GetTypes()) {
          base::UmaHistogramSparse(
              "Autocomplete.KeyMetrics.FillingAcceptance.ByFieldType",
              GetBucketForAcceptanceMetricsGroupedByFieldType(
                  field_type, any_suggestion_accepted));
        }
      }
      if (is_label_sensitive_enabled) {
        for (MatchingType matching_type : state.shown_matching_types) {
          bool is_accepted = any_suggestion_accepted &&
                             state.accepted_matching_type == matching_type;
          base::UmaHistogramBoolean(
              base::StrCat({"Autocomplete.KeyMetrics.FillingAcceptance",
                            MatchingTypeToSuffix(matching_type)}),
              is_accepted);
        }
      }
    }

    if (any_suggestion_accepted) {
      bool is_correct = state.filled_value == field.value();
      base::UmaHistogramBoolean("Autocomplete.KeyMetrics.FillingCorrectness",
                                is_correct);
      if (is_label_sensitive_enabled && state.accepted_matching_type) {
        base::UmaHistogramBoolean(
            base::StrCat({"Autocomplete.KeyMetrics.FillingCorrectness",
                          MatchingTypeToSuffix(*state.accepted_matching_type)}),
            is_correct);
      }
    }
  }
}

}  // namespace autofill::autofill_metrics
