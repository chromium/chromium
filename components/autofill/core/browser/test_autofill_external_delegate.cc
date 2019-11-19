// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_external_delegate.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestAutofillExternalDelegate::TestAutofillExternalDelegate(
    AutofillManager* autofill_manager,
    AutofillDriver* autofill_driver,
    bool call_parent_methods)
    : AutofillExternalDelegate(autofill_manager, autofill_driver),
      call_parent_methods_(call_parent_methods) {}

TestAutofillExternalDelegate::~TestAutofillExternalDelegate() {}

void TestAutofillExternalDelegate::OnPopupShown() {
  popup_hidden_ = false;

  AutofillExternalDelegate::OnPopupShown();
}

void TestAutofillExternalDelegate::OnPopupHidden() {
  popup_hidden_ = true;

  run_loop_.Quit();
}

void TestAutofillExternalDelegate::OnQuery(int query_id,
                                           const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounds) {
  on_query_seen_ = true;
  on_suggestions_returned_seen_ = false;

  // If necessary, call the superclass's OnQuery to set up its other fields
  // properly.
  if (call_parent_methods_)
    AutofillExternalDelegate::OnQuery(query_id, form, field, bounds);
}

void TestAutofillExternalDelegate::OnSuggestionsReturned(
    int query_id,
    const std::vector<Suggestion>& suggestions,
    bool autoselect_first_suggestion,
    bool is_all_server_suggestions) {
  on_suggestions_returned_seen_ = true;
  query_id_ = query_id;
  suggestions_ = suggestions;
  autoselect_first_suggestion_ = autoselect_first_suggestion;
  is_all_server_suggestions_ = is_all_server_suggestions;

  // If necessary, call the superclass's OnSuggestionsReturned in order to
  // execute logic relating to showing the popup or not.
  if (call_parent_methods_)
    AutofillExternalDelegate::OnSuggestionsReturned(query_id, suggestions,
                                                    is_all_server_suggestions);
}

bool TestAutofillExternalDelegate::HasActiveScreenReader() const {
  return has_active_screen_reader_;
}

void TestAutofillExternalDelegate::OnAutofillAvailabilityEvent(
    const mojom::AutofillState state) {
  if (state == mojom::AutofillState::kAutofillAvailable)
    has_suggestions_available_on_field_focus_ = true;
  else if (state == mojom::AutofillState::kNoSuggestions)
    has_suggestions_available_on_field_focus_ = false;
}

void TestAutofillExternalDelegate::WaitForPopupHidden() {
  if (popup_hidden_)
    return;

  run_loop_.Run();
}

void TestAutofillExternalDelegate::CheckSuggestions(
    int expected_page_id,
    size_t expected_num_suggestions,
    const Suggestion expected_suggestions[]) {
  // Ensure that these results are from the most recent query.
  EXPECT_TRUE(on_suggestions_returned_seen_);

  EXPECT_EQ(expected_page_id, query_id_);
  ASSERT_LE(expected_num_suggestions, suggestions_.size());
  for (size_t i = 0; i < expected_num_suggestions; ++i) {
    SCOPED_TRACE(base::StringPrintf("i: %" PRIuS, i));
    EXPECT_EQ(expected_suggestions[i].value, suggestions_[i].value);
    EXPECT_EQ(expected_suggestions[i].label, suggestions_[i].label);
    EXPECT_EQ(expected_suggestions[i].icon, suggestions_[i].icon);
    EXPECT_EQ(expected_suggestions[i].frontend_id, suggestions_[i].frontend_id);
  }
  ASSERT_EQ(expected_num_suggestions, suggestions_.size());
}

void TestAutofillExternalDelegate::CheckNoSuggestions(int expected_page_id) {
  CheckSuggestions(expected_page_id, 0, nullptr);
}

void TestAutofillExternalDelegate::CheckSuggestionCount(
    int expected_page_id,
    size_t expected_num_suggestions) {
  // Ensure that these results are from the most recent query.
  EXPECT_TRUE(on_suggestions_returned_seen_);

  EXPECT_EQ(expected_page_id, query_id_);
  ASSERT_EQ(expected_num_suggestions, suggestions_.size());
}

bool TestAutofillExternalDelegate::on_query_seen() const {
  return on_query_seen_;
}

bool TestAutofillExternalDelegate::on_suggestions_returned_seen() const {
  return on_suggestions_returned_seen_;
}

bool TestAutofillExternalDelegate::autoselect_first_suggestion() const {
  return autoselect_first_suggestion_;
}

bool TestAutofillExternalDelegate::is_all_server_suggestions() const {
  return is_all_server_suggestions_;
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
