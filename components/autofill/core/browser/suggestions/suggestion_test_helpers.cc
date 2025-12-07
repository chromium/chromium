// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"

namespace autofill {
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Matcher;

namespace {
Matcher<Suggestion::Text> EqualsTextPrimary(const bool is_primary) {
  return Field(&Suggestion::Text::is_primary,
               Suggestion::Text::IsPrimary(is_primary));
}
}  // namespace

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
                                     const Suggestion::Payload& payload) {
  return AllOf(EqualsSuggestion(id), Field(&Suggestion::payload, payload));
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

Matcher<Suggestion> EqualsSuggestion(
    SuggestionType type,
    const std::u16string& main_text,
    const bool is_main_text_primary,
    Suggestion::Icon icon,
    const std::vector<std::vector<Suggestion::Text>>& labels,
    const Suggestion::Payload& payload) {
  return AllOf(
      EqualsSuggestion(type, main_text, icon),
      Field(&Suggestion::labels, labels), Field(&Suggestion::payload, payload),
      Field(&Suggestion::main_text, EqualsTextPrimary(is_main_text_primary)));
}

Matcher<Suggestion> EqualsSuggestion(
    SuggestionType type,
    const std::u16string& main_text,
    const bool is_main_text_primary,
    Suggestion::LetterMonochromeIcon letter_icon,
    const std::vector<std::vector<Suggestion::Text>>& labels,
    const Suggestion::Payload& payload) {
  return AllOf(
      EqualsSuggestion(type, main_text), Field(&Suggestion::labels, labels),
      Field(&Suggestion::payload, payload),
      Field(&Suggestion::main_text, EqualsTextPrimary(is_main_text_primary)),
      Field(&Suggestion::custom_icon, letter_icon));
}

Matcher<Suggestion> HasIcon(Suggestion::Icon icon) {
  return Field(&Suggestion::icon, icon);
}

Matcher<Suggestion> HasTrailingIcon(Suggestion::Icon icon) {
  return Field(&Suggestion::trailing_icon, icon);
}

Matcher<Suggestion> HasIphFeature(
    raw_ptr<const base::Feature> feature_for_iph) {
  return Field(&Suggestion::iph_metadata,
               Field(&Suggestion::IPHMetadata::feature, feature_for_iph));
}

Matcher<Suggestion> HasNoIphFeature() {
  return Field(&Suggestion::iph_metadata,
               Field(&Suggestion::IPHMetadata::feature, nullptr));
}

}  // namespace autofill
