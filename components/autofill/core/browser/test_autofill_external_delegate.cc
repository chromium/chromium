// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_external_delegate.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

TestAutofillExternalDelegate::TestAutofillExternalDelegate(
    BrowserAutofillManager* autofill_manager,
    bool call_parent_methods)
    : AutofillExternalDelegate(autofill_manager),
      call_parent_methods_(call_parent_methods) {}

TestAutofillExternalDelegate::~TestAutofillExternalDelegate() = default;

void TestAutofillExternalDelegate::OnSuggestionsShown(
    base::span<const Suggestion> suggestions) {
  popup_hidden_ = false;

  AutofillExternalDelegate::OnSuggestionsShown(suggestions);
}

void TestAutofillExternalDelegate::OnSuggestionsHidden() {
  popup_hidden_ = true;

  run_loop_.Quit();
}

void TestAutofillExternalDelegate::OnQuery(
    const FormData& form,
    const FormFieldData& field,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source) {
  on_query_seen_ = true;
  on_suggestions_returned_seen_ = false;
  trigger_source_ = trigger_source;

  // If necessary, call the superclass's OnQuery to set up its other fields
  // properly.
  if (call_parent_methods_)
    AutofillExternalDelegate::OnQuery(form, field, caret_bounds,
                                      trigger_source);
}

void TestAutofillExternalDelegate::OnSuggestionsReturned(
    FieldGlobalId field_id,
    const std::vector<Suggestion>& suggestions,
    std::optional<autofill_metrics::SuggestionRankingContext>
        suggestion_ranking_context) {
  on_suggestions_returned_seen_ = true;
  field_id_ = field_id;
  suggestions_ = suggestions;
  suggestion_ranking_context_ = suggestion_ranking_context;

  // If necessary, call the superclass's OnSuggestionsReturned in order to
  // execute logic relating to showing the popup or not.
  if (call_parent_methods_) {
    AutofillExternalDelegate::OnSuggestionsReturned(field_id, suggestions,
                                                    suggestion_ranking_context);
  }
}

bool TestAutofillExternalDelegate::HasActiveScreenReader() const {
  return has_active_screen_reader_;
}

void TestAutofillExternalDelegate::OnAutofillAvailabilityEvent(
    mojom::AutofillSuggestionAvailability suggestion_availability) {
  if (suggestion_availability ==
      mojom::AutofillSuggestionAvailability::kAutofillAvailable) {
    has_suggestions_available_on_field_focus_ = true;
  } else if (suggestion_availability ==
             mojom::AutofillSuggestionAvailability::kNoSuggestions) {
    has_suggestions_available_on_field_focus_ = false;
  }
}

void TestAutofillExternalDelegate::WaitForPopupHidden() {
  if (popup_hidden_)
    return;

  run_loop_.Run();
}

void TestAutofillExternalDelegate::CheckSuggestions(
    FieldGlobalId field_id,
    const std::vector<Suggestion>& expected_suggestions) {
  // Ensure that these results are from the most recent query.
  EXPECT_TRUE(on_suggestions_returned_seen_);

  EXPECT_EQ(field_id, field_id_);
  ASSERT_EQ(expected_suggestions.size(), suggestions_.size());
  for (size_t i = 0; i < suggestions_.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("i: %" PRIuS, i));
    EXPECT_EQ(expected_suggestions[i].main_text.value,
              suggestions_[i].main_text.value);
    EXPECT_EQ(expected_suggestions[i].minor_text.value,
              suggestions_[i].minor_text.value);
    EXPECT_EQ(expected_suggestions[i].labels, suggestions_[i].labels);
    EXPECT_EQ(expected_suggestions[i].icon, suggestions_[i].icon);
    EXPECT_EQ(expected_suggestions[i].type, suggestions_[i].type);
    EXPECT_EQ(expected_suggestions[i].is_acceptable,
              suggestions_[i].is_acceptable);
  }
}

void TestAutofillExternalDelegate::CheckSuggestionsNotReturned(
    FieldGlobalId field_id) {
  if (on_suggestions_returned_seen_) {
    EXPECT_NE(field_id, field_id_);
  }
}

void TestAutofillExternalDelegate::CheckNoSuggestions(FieldGlobalId field_id) {
  CheckSuggestions(field_id, {});
}

void TestAutofillExternalDelegate::CheckSuggestionCount(
    FieldGlobalId field_id,
    size_t expected_num_suggestions) {
  // Ensure that these results are from the most recent query.
  EXPECT_TRUE(on_suggestions_returned_seen_);

  EXPECT_EQ(field_id, field_id_);
  ASSERT_EQ(expected_num_suggestions, suggestions_.size());
}

const std::vector<Suggestion>& TestAutofillExternalDelegate::suggestions()
    const {
  return suggestions_;
}

bool TestAutofillExternalDelegate::on_query_seen() const {
  return on_query_seen_;
}

bool TestAutofillExternalDelegate::on_suggestions_returned_seen() const {
  return on_suggestions_returned_seen_;
}

AutofillSuggestionTriggerSource TestAutofillExternalDelegate::trigger_source()
    const {
  return trigger_source_;
}

bool TestAutofillExternalDelegate::popup_hidden() const {
  return popup_hidden_;
}

void TestAutofillExternalDelegate::set_has_active_screen_reader(
    bool has_active_screen_reader) {
  has_active_screen_reader_ = has_active_screen_reader;
}

bool TestAutofillExternalDelegate::has_suggestions_available_on_field_focus()
    const {
  return has_suggestions_available_on_field_focus_;
}

}  // namespace autofill
