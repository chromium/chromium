// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestion_util.h"

#include <vector>

#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Matcher;

// Matches a suggestion in a loading state.
Matcher<Suggestion> IsLoadingSuggestion(
    const std::u16string& main_text,
    Matcher<std::vector<Suggestion>> children_matcher = ElementsAre(),
    SuggestionType type = SuggestionType::kAddressEntry) {
  return AllOf(
      EqualsSuggestion(type, main_text),
      Field(&Suggestion::acceptability,
            Suggestion::Acceptability::kSelectableButUnacceptable),
      Field(&Suggestion::is_loading, Suggestion::IsLoading(true)),
      Field(&Suggestion::children, children_matcher));
}

// Matches a deactivated suggestion.
Matcher<Suggestion> IsDisabledSuggestion(
    const std::u16string& main_text,
    Matcher<std::vector<Suggestion>> children_matcher = ElementsAre(),
    SuggestionType type = SuggestionType::kAddressEntry) {
  return AllOf(
      EqualsSuggestion(type, main_text),
      Field(&Suggestion::acceptability,
            Suggestion::Acceptability::kUnselectableAndUnacceptable),
      Field(&Suggestion::is_loading, Suggestion::IsLoading(false)),
      Field(&Suggestion::children, children_matcher));
}

// Tests that selecting a root-level suggestion marks it as loading and disables
// all other suggestions and children.
TEST(SuggestionUtilTest, PrepareLoadingStateSuggestions_RootSelected) {
  Suggestion selected_suggestion(u"Selected", SuggestionType::kAddressEntry);
  Suggestion other_suggestion(u"Other", SuggestionType::kAddressEntry);
  Suggestion child_suggestion(u"Child", SuggestionType::kAddressEntry);
  selected_suggestion.children = {child_suggestion};

  const std::vector<Suggestion> result = PrepareLoadingStateSuggestions(
      {selected_suggestion, other_suggestion}, selected_suggestion);

  EXPECT_THAT(
      result,
      ElementsAre(
          IsLoadingSuggestion(u"Selected",
                              ElementsAre(IsDisabledSuggestion(u"Child"))),
          IsDisabledSuggestion(u"Other")));
}

// Tests that selecting a child suggestion marks its root-level parent as
// loading and disables all child suggestions.
TEST(SuggestionUtilTest, PrepareLoadingStateSuggestions_ChildSelected) {
  Suggestion child_suggestion(u"Child", SuggestionType::kAddressEntry);
  Suggestion parent_suggestion(u"Parent", SuggestionType::kAddressEntry);
  parent_suggestion.children = {child_suggestion};
  Suggestion other_suggestion(u"Other", SuggestionType::kAddressEntry);

  const std::vector<Suggestion> result = PrepareLoadingStateSuggestions(
      {parent_suggestion, other_suggestion}, child_suggestion);

  EXPECT_THAT(
      result,
      ElementsAre(
          IsLoadingSuggestion(u"Parent",
                              ElementsAre(IsDisabledSuggestion(u"Child"))),
          IsDisabledSuggestion(u"Other")));
}

// Tests that when the selected suggestion is not present, all root-level
// suggestions and children are disabled.
TEST(SuggestionUtilTest, PrepareLoadingStateSuggestions_NoneSelected) {
  Suggestion child_suggestion(u"Child", SuggestionType::kAddressEntry);
  Suggestion parent_suggestion(u"Parent", SuggestionType::kAddressEntry);
  parent_suggestion.children = {child_suggestion};
  Suggestion other_suggestion(u"Other", SuggestionType::kAddressEntry);
  Suggestion unselected_suggestion(u"Unselected",
                                   SuggestionType::kAddressEntry);

  const std::vector<Suggestion> result = PrepareLoadingStateSuggestions(
      {parent_suggestion, other_suggestion}, unselected_suggestion);

  EXPECT_THAT(
      result,
      ElementsAre(
          IsDisabledSuggestion(u"Parent",
                               ElementsAre(IsDisabledSuggestion(u"Child"))),
          IsDisabledSuggestion(u"Other")));
}

}  // namespace
}  // namespace autofill
