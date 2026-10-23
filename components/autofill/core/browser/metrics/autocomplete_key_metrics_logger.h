// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOCOMPLETE_KEY_METRICS_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOCOMPLETE_KEY_METRICS_LOGGER_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table_label_sensitive.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace autofill {

class FormData;
class FormStructure;
struct Suggestion;

namespace autofill_metrics {

// Tracks and records key metrics (FillingAcceptance and FillingCorrectness) for
// autocomplete suggestions per field, in aggregate, broken down by
// MatchingType, and grouped by field type.
//
// One instance per `BrowserAutofillManager`, owned by
// `BrowserAutofillManager::MetricsState` and recreated when
// `BrowserAutofillManager` is reset (e.g., on page navigation).
//
// If a user accepts multiple autocomplete suggestions on the same field before
// form submission, only the last accepted suggestion (its filled value and
// MatchingType) is evaluated for the per-MatchingType acceptance breakdown and
// for FillingCorrectness upon submission.
class AutocompleteKeyMetricsLogger {
 public:
  AutocompleteKeyMetricsLogger();
  AutocompleteKeyMetricsLogger(const AutocompleteKeyMetricsLogger&) = delete;
  AutocompleteKeyMetricsLogger& operator=(const AutocompleteKeyMetricsLogger&) =
      delete;
  ~AutocompleteKeyMetricsLogger();

  // Called when suggestions are shown for a field. Records all distinct
  // autocomplete matching types present in `suggestions` for `field_id`.
  void OnSuggestionsShown(FieldGlobalId field_id,
                          base::span<const Suggestion> suggestions);

  // Called when an autocomplete suggestion is selected for a field. Overwrites
  // any previously recorded accepted value and matching type for `field_id`.
  void OnSuggestionSelected(FieldGlobalId field_id,
                            const Suggestion& suggestion);

  // Called upon form submission to emit FillingAcceptance and
  // FillingCorrectness metrics for each field in `form`. If `form_structure` is
  // non-null, also emits FillingAcceptance by field type for fields present in
  // `form_structure`.
  void OnFormSubmitted(const FormData& form,
                       const FormStructure* form_structure);

 private:
  struct FieldState {
    FieldState();
    FieldState(const FieldState&);
    FieldState& operator=(const FieldState&);
    FieldState(FieldState&&);
    FieldState& operator=(FieldState&&);
    ~FieldState();

    bool any_suggestion_shown = false;
    DenseSet<MatchingType> shown_matching_types;

    std::optional<MatchingType> accepted_matching_type;
    // Set to the accepted suggestion's value when an autocomplete suggestion is
    // selected for this field. `filled_value.has_value()` indicates whether any
    // autocomplete suggestion was accepted.
    std::optional<std::u16string> filled_value;
  };

  absl::flat_hash_map<FieldGlobalId, FieldState> field_states_;
};

}  // namespace autofill_metrics
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOCOMPLETE_KEY_METRICS_LOGGER_H_
