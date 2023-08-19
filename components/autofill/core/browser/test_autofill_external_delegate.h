// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_

#include <vector>

#include "base/run_loop.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"

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
  void OnPopupShown() override;
  void OnPopupHidden() override;
  void OnQuery(const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounds) override;
  void OnSuggestionsReturned(FieldGlobalId field_id,
                             const std::vector<Suggestion>& suggestions,
                             AutofillSuggestionTriggerSource trigger_source,
                             bool is_all_server_suggestions) override;
  bool HasActiveScreenReader() const override;
  void OnAutofillAvailabilityEvent(const mojom::AutofillState state) override;

  // Functions unique to TestAutofillExternalDelegate.

  void WaitForPopupHidden();

  void CheckSuggestions(FieldGlobalId field_id,
                        size_t expected_num_suggestions,
                        const Suggestion expected_suggestions[]);

  // Check that the autofill suggestions were sent, and that they match a page
  // but contain no results.
  void CheckNoSuggestions(FieldGlobalId field_id);

  // Check that the autofill suggestions were sent, and that they match a page
  // and contain a specific number of suggestions.
  void CheckSuggestionCount(FieldGlobalId field_id,
                            size_t expected_num_suggestions);

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

  // Records whether the Autofill suggestions all come from Google Payments.
  bool is_all_server_suggestions_ = false;

  // The field id of the most recent Autofill query.
  FieldGlobalId field_id_;

  // The results returned by the most recent Autofill query.
  std::vector<Suggestion> suggestions_;

  // |true| if the popup is hidden, |false| if the popup is shown.
  bool popup_hidden_ = true;

  bool has_active_screen_reader_ = true;

  bool has_suggestions_available_on_field_focus_ = false;

  base::RunLoop run_loop_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_
