// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_LOGGER_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_LOGGER_H_

#include <map>

#include "components/autofill/core/common/unique_ids.h"

namespace autofill_ai {

// A class that takes care of keeping track of metric-related states and user
// interactions with forms.
class AutofillAiLogger {
 public:
  AutofillAiLogger();
  AutofillAiLogger(const AutofillAiLogger&) = delete;
  AutofillAiLogger& operator=(const AutofillAiLogger&) = delete;
  ~AutofillAiLogger();

  void OnFormEligibilityAvailable(autofill::FormGlobalId form_id,
                                  bool is_eligible);
  void OnFormHasDataToFill(autofill::FormGlobalId form_id);
  void OnSuggestionsShown(autofill::FormGlobalId form_id);
  void OnTriggeredFillingSuggestions(autofill::FormGlobalId form_id);
  void OnFillingSuggestionsShown(autofill::FormGlobalId form_id);
  void OnDidFillSuggestion(autofill::FormGlobalId form_id);
  void OnDidCorrectFillingSuggestion(autofill::FormGlobalId form_id);

  // Function that records the contents of `form_states` for `form_id` into
  // appropriate metrics. `submission_state` denotes whether the form was
  // submitted or abandoned.
  void RecordMetricsForForm(autofill::FormGlobalId form_id,
                            bool submission_state);

 private:
  // Helper struct that contains relevant information about the state of a form
  // regarding the prediction improvement system.
  // TODO(crbug.com/372170223): Investigate whether this can be represented as
  // an enum.
  struct FunnelState {
    // Given a form, records whether it is supported for filling by prediction
    // improvements.
    bool is_eligible = false;
    // Given a form, records whether there's data available to fill this form.
    // Whether or not this data is used for filling is irrelevant.
    bool has_data_to_fill = false;
    // Given a form, records whether prediction improvement suggestions were
    // shown for this form.
    bool did_show_suggestions = false;
    // Given a form, records whether the user triggered prediction improvement
    // suggestions, which started loading filling suggestions.
    bool did_start_loading_suggestions = false;
    // Given a form, records whether filling suggestions were actually shown
    // to the user.
    bool did_show_filling_suggestions = false;
    // Given a form, records whether the user chose to fill the form with a
    // filling suggestion.
    bool did_fill_suggestions = false;
    // Given a form, records whether the user corrected fields filled using
    // prediction improvements filling suggestions.
    bool did_correct_filling = false;
  };
  // Records the funnel state of each form. See the documentation of
  // `FunnelState` for more information about what is recorded.
  std::map<autofill::FormGlobalId, FunnelState> form_states_;
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_LOGGER_H_
