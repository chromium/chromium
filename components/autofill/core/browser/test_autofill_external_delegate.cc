// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_external_delegate.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestAutofillExternalDelegate::TestAutofillExternalDelegate(
    BrowserAutofillManager* autofill_manager,
    bool call_parent_methods)
    : AutofillExternalDelegate(autofill_manager),
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

void TestAutofillExternalDelegate::OnQuery(const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounds) {
  on_query_seen_ = true;
  on_suggestions_returned_seen_ = false;

  // If necessary, call the superclass's OnQuery to set up its other fields
  // properly.
  if (call_parent_methods_)
    AutofillExternalDelegate::OnQuery(form, field, bounds);
}

void TestAutofillExternalDelegate::OnSuggestionsReturned(
    FieldGlobalId field_id,
    const std::vector<Suggestion>& suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    bool is_all_server_suggestions) {
  on_suggestions_returned_seen_ = true;
  field_id_ = field_id;
  suggestions_ = suggestions;
  trigger_source_ = trigger_source;
  is_all_server_suggestions_ = is_all_server_suggestions;

  // If necessary, call the superclass's OnSuggestionsReturned in order to
  // execute logic relating to showing the popup or not.
  if (call_parent_methods_)
    AutofillExternalDelegate::OnSuggestionsReturned(
        field_id, suggestions, trigger_source, is_all_server_suggestions);
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
    FieldGlobalId field_id,
    size_t expected_num_suggestions,
    const Suggestion expected_suggestions[]) {
  // Ensure that these results are from the most recent query.
  EXPECT_TRUE(on_suggestions_returned_seen_);

  EXPECT_EQ(field_id, field_id_);
  ASSERT_LE(expected_num_suggestions, suggestions_.size());
  for (size_t i = 0; i < expected_num_suggestions; ++i) {
    SCOPED_TRACE(base::StringPrintf("i: %" PRIuS, i));
    EXPECT_EQ(expected_suggestions[i].main_text.value,
              suggestions_[i].main_text.value);
    EXPECT_EQ(expected_suggestions[i].minor_text.value,
              suggestions_[i].minor_text.value);
    EXPECT_EQ(expected_suggestions[i].labels, suggestions_[i].labels);
    EXPECT_EQ(expected_suggestions[i].icon, suggestions_[i].icon);
    EXPECT_EQ(expected_suggestions[i].popup_item_id,
              suggestions_[i].popup_item_id);
  }
  ASSERT_EQ(expected_num_suggestions, suggestions_.size());
}

void TestAutofillExternalDelegate::CheckNoSuggestions(FieldGlobalId field_id) {
  CheckSuggestions(field_id, 0, nullptr);
}

void TestAutofillExternalDelegate::CheckSuggestionCount(
    FieldGlobalId field_id,
    size_t expected_num_suggestions) {
  // Ensure that these results are from the most recent query.
  EXPECT_TRUE(on_suggestions_returned_seen_);

  EXPECT_EQ(field_id, field_id_);
  ASSERT_EQ(expected_num_suggestions, suggestions_.size());
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
