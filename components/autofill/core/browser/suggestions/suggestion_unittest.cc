// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestion.h"

#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

TEST(SuggestionTest, IsAcceptable) {
  // Acceptable suggestion types with default (kSelectableAndAcceptable)
  // acceptability.
  EXPECT_TRUE(Suggestion(SuggestionType::kAddressEntry).IsAcceptable());
  EXPECT_TRUE(Suggestion(SuggestionType::kCreditCardEntry).IsAcceptable());
  EXPECT_TRUE(Suggestion(SuggestionType::kIbanEntry).IsAcceptable());
  EXPECT_TRUE(Suggestion(SuggestionType::kAutocompleteEntry).IsAcceptable());

  // Unacceptable suggestion types return false regardless of acceptability.
  EXPECT_FALSE(Suggestion(SuggestionType::kSeparator).IsAcceptable());
  EXPECT_FALSE(Suggestion(SuggestionType::kTitle).IsAcceptable());
  EXPECT_FALSE(Suggestion(SuggestionType::kMixedFormMessage).IsAcceptable());
  EXPECT_FALSE(
      Suggestion(SuggestionType::kInsecureContextPaymentDisabledMessage)
          .IsAcceptable());
  EXPECT_FALSE(
      Suggestion(SuggestionType::kAtMemorySourceAttribution).IsAcceptable());

  // Non-kSelectableAndAcceptable acceptability states return false for
  // acceptable types.
  using enum Suggestion::Acceptability;
  Suggestion acceptable_suggestion(SuggestionType::kAddressEntry);
  EXPECT_TRUE(acceptable_suggestion.IsAcceptable());

  Suggestion unacceptable_suggestion(SuggestionType::kAddressEntry);
  unacceptable_suggestion.acceptability = kSelectableButUnacceptable;
  EXPECT_FALSE(unacceptable_suggestion.IsAcceptable());

  Suggestion unselectable_suggestion(SuggestionType::kAddressEntry);
  unselectable_suggestion.acceptability = kUnselectableAndUnacceptable;
  EXPECT_FALSE(unselectable_suggestion.IsAcceptable());

  EXPECT_TRUE(acceptable_suggestion.IsSelectable());
  EXPECT_TRUE(unacceptable_suggestion.IsSelectable());
  EXPECT_FALSE(unselectable_suggestion.IsSelectable());
}

}  // namespace
}  // namespace autofill
