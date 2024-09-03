// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_

#include <vector>

#include "base/run_loop.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace autofill {

class TestAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  explicit TestAutofillExternalDelegate(
      BrowserAutofillManager* autofill_manager,
      bool call_parent_methods);

  TestAutofillExternalDelegate(const TestAutofillExternalDelegate&) = delete;
  TestAutofillExternalDelegate& operator=(const TestAutofillExternalDelegate&) =
      delete;

  ~TestAutofillExternalDelegate() override;

  // AutofillExternalDelegate overrides.
  void OnSuggestionsShown(base::span<const Suggestion> suggestions) override;
  void OnSuggestionsHidden() override;
  void OnQuery(const FormData& form,
               const FormFieldData& field,
               const gfx::Rect& caret_bounds,
               AutofillSuggestionTriggerSource trigger_source) override;
  void OnSuggestionsReturned(
      FieldGlobalId field_id,
      const std::vector<Suggestion>& suggestions,
      std::optional<autofill_metrics::SuggestionRankingContext>
          suggestion_ranking_context =
              autofill_metrics::SuggestionRankingContext()) override;
  bool HasActiveScreenReader() const override;
  void OnAutofillAvailabilityEvent(
      mojom::AutofillSuggestionAvailability suggestion_availability) override;

  // Functions unique to TestAutofillExternalDelegate.

  void WaitForPopupHidden();

  void CheckSuggestions(FieldGlobalId field_id,
                        const std::vector<Suggestion>& expected_sugestions);

  // Check that the autofill suggestions were not sent at all.
  void CheckSuggestionsNotReturned(FieldGlobalId field_id);

  // Check that the autofill suggestions were sent, and that they match a page
  // but contain no results.
  void CheckNoSuggestions(FieldGlobalId field_id);

  // Check that the autofill suggestions were sent, and that they match a page
  // and contain a specific number of suggestions.
  void CheckSuggestionCount(FieldGlobalId field_id,
                            size_t expected_num_suggestions);

  const std::vector<Suggestion>& suggestions() const;

  bool on_query_seen() const;

  bool on_suggestions_returned_seen() const;

  AutofillSuggestionTriggerSource trigger_source() const;

  bool is_all_server_suggestions() const;

  bool popup_hidden() const;

  void set_has_active_screen_reader(bool has_active_screen_reader);

  bool has_suggestions_available_on_field_focus() const;

 private:
  // If true, calls AutofillExternalDelegate::OnQuery and
  // AutofillExternalDelegate::OnSuggestionsReturned.
  bool call_parent_methods_;

  // Records if OnQuery has been called yet.
  bool on_query_seen_ = false;

  // Records if OnSuggestionsReturned has been called after the most recent
  // call to OnQuery.
  bool on_suggestions_returned_seen_ = false;

  // Records the trigger source of `OnSuggestionsReturned()`.
  AutofillSuggestionTriggerSource trigger_source_ =
      AutofillSuggestionTriggerSource::kUnspecified;

  // The field id of the most recent Autofill query.
  FieldGlobalId field_id_;

  // The results returned by the most recent Autofill query.
  std::vector<Suggestion> suggestions_;

  // Contains information on the ranking of suggestions using the new and old
  // ranking algorithm. Used for metrics logging.
  std::optional<autofill_metrics::SuggestionRankingContext>
      suggestion_ranking_context_;

  // |true| if the popup is hidden, |false| if the popup is shown.
  bool popup_hidden_ = true;

  bool has_active_screen_reader_ = true;

  bool has_suggestions_available_on_field_focus_ = false;

  base::RunLoop run_loop_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_
