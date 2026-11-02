// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/form_suggestion_conversion.h"

#import <optional>
#import <vector>

#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#import "components/autofill/core/common/autofill_test_util.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace autofill {
namespace {

using ::testing::NiceMock;

class FormSuggestionConversionTest : public PlatformTest {
 protected:
  test::AutofillUnitTestEnvironment autofill_environment_;
  NiceMock<MockAutofillSuggestionDelegate> delegate_;
  const FormGlobalId form_id_ = test::MakeFormGlobalId();
  const FieldGlobalId field_id_ = test::MakeFieldGlobalId();
};

// Test that a fillable suggestion is converted with its values and metadata.
TEST_F(FormSuggestionConversionTest, ConvertsFillableSuggestion) {
  Suggestion suggestion(u"John Doe", SuggestionType::kAddressEntry);
  suggestion.labels = {{Suggestion::Text(u"1 Main St")}};

  FormSuggestion* form_suggestion = FormSuggestionFromAutofillSuggestion(
      suggestion, /*index=*/3, delegate_.GetWeakPtr(), form_id_, field_id_);

  ASSERT_NSNE(nil, form_suggestion);
  EXPECT_NSEQ(@"John Doe", form_suggestion.value);
  EXPECT_NSEQ(@"1 Main St", form_suggestion.displayDescription);
  EXPECT_EQ(SuggestionType::kAddressEntry, form_suggestion.type);
  EXPECT_EQ(std::optional<size_t>(3),
            form_suggestion.metadata.popup_suggestion_index);
  EXPECT_EQ(form_id_, form_suggestion.metadata.form_id);
  EXPECT_EQ(field_id_, form_suggestion.metadata.field_id);
  EXPECT_EQ(&delegate_, form_suggestion.metadata.suggestion_delegate.get());
}

// Test that suggestions the keyboard accessory can't display aren't converted.
TEST_F(FormSuggestionConversionTest, SkipsNonDisplayableSuggestion) {
  EXPECT_NSEQ(nil, FormSuggestionFromAutofillSuggestion(
                       Suggestion(SuggestionType::kSeparator), /*index=*/0,
                       delegate_.GetWeakPtr(), form_id_, field_id_));
}

// Test that the IPH feature of a suggestion is mapped.
TEST_F(FormSuggestionConversionTest, MapsIPHFeature) {
  Suggestion suggestion(u"John Doe", SuggestionType::kAddressEntry);
  suggestion.iph_metadata = Suggestion::IPHMetadata(
      &feature_engagement::kIPHAutofillExternalAccountProfileSuggestionFeature);

  FormSuggestion* form_suggestion = FormSuggestionFromAutofillSuggestion(
      suggestion, /*index=*/0, delegate_.GetWeakPtr(), form_id_, field_id_);

  EXPECT_EQ(SuggestionFeatureForIPH::kAutofillExternalAccountProfile,
            form_suggestion.featureForIPH);
}

// Test that converting a list skips non-displayable suggestions, moves Undo
// first and keeps each suggestion's original index.
TEST_F(FormSuggestionConversionTest, ConvertsListWithUndoFirst) {
  // The converter ignores display text for kUndo, so the icon is what keeps
  // it from being skipped.
  Suggestion undo(u"Undo", SuggestionType::kUndo);
  undo.icon = Suggestion::Icon::kUndo;
  std::vector<Suggestion> suggestions = {
      Suggestion(u"John Doe", SuggestionType::kAddressEntry),
      Suggestion(SuggestionType::kSeparator),
      undo,
  };

  NSArray<FormSuggestion*>* form_suggestions =
      FormSuggestionsFromAutofillSuggestions(suggestions,
                                             delegate_.GetWeakPtr(), form_id_,
                                             field_id_, /*client=*/nullptr);

  ASSERT_EQ(2U, form_suggestions.count);
  EXPECT_EQ(SuggestionType::kUndo, form_suggestions[0].type);
  EXPECT_EQ(std::optional<size_t>(2),
            form_suggestions[0].metadata.popup_suggestion_index);
  EXPECT_EQ(SuggestionType::kAddressEntry, form_suggestions[1].type);
  EXPECT_EQ(std::optional<size_t>(0),
            form_suggestions[1].metadata.popup_suggestion_index);
}

}  // namespace
}  // namespace autofill
