// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"

namespace autofill {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::Matcher;

Matcher<Suggestion> EqualsSuggestion(SuggestionType id) {
  return Field(&Suggestion::type, id);
}

Matcher<Suggestion> EqualsSuggestion(SuggestionType id,
                                     const std::u16string& main_text) {
  return AllOf(
      Field(&Suggestion::type, id),
      Field(&Suggestion::main_text,
            Suggestion::Text(main_text, Suggestion::Text::IsPrimary(true))));
}

Matcher<Suggestion> EqualsSuggestion(SuggestionType id,
                                     const std::u16string& main_text,
                                     Suggestion::Icon icon) {
  return AllOf(EqualsSuggestion(id, main_text), Field(&Suggestion::icon, icon));
}

Matcher<Suggestion> EqualsSuggestion(
    SuggestionType type,
    const std::u16string& main_text,
    Suggestion::Icon icon,
    const std::vector<std::vector<Suggestion::Text>>& labels) {
  return AllOf(EqualsSuggestion(type, main_text, icon),
               Field(&Suggestion::labels, labels));
}

Matcher<Suggestion> EqualsSuggestion(SuggestionType id,
                                     const std::u16string& main_text,
                                     Suggestion::Icon icon,
                                     const Suggestion::Payload& payload) {
  return AllOf(EqualsSuggestion(id, main_text, icon),
               Field(&Suggestion::payload, payload));
}

}  // namespace autofill
