// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::features::kAutofillFixFillableFieldTypes;

namespace autofill {

namespace {
FieldRendererId MakeFieldRendererId() {
  static uint64_t id_counter_ = 0;
  return FieldRendererId(++id_counter_);
}

// Sets both the field label and parseable label to |label|.
void SetFieldLabels(AutofillField* field, const std::u16string& label) {
  field->label = label;
  field->set_parseable_label(label);
}

}  // namespace

class FormFieldTest
    : public testing::TestWithParam<std::tuple<bool, PatternSource>> {
 public:
  FormFieldTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kAutofillParsingPatternProvider,
        enable_parsing_pattern_provider());
  }
  FormFieldTest(const FormFieldTest&) = delete;
  FormFieldTest& operator=(const FormFieldTest&) = delete;
  ~FormFieldTest() override = default;

  bool enable_parsing_pattern_provider() const {
    return std::get<0>(GetParam());
  }

  PatternSource pattern_source() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(FormFieldTest,
                         FormFieldTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Values(
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_HEADERS)
                                                PatternSource::kDefault,
                                                PatternSource::kExperimental,
                                                PatternSource::kNextGen,
#endif
                                                PatternSource::kLegacy)));

struct MatchTestCase {
  std::u16string label;
  std::vector<std::u16string> positive_patterns;
  std::vector<std::u16string> negative_patterns;
};

class MatchTest : public testing::TestWithParam<MatchTestCase> {};

const MatchTestCase kMatchTestCases[]{
    // Empty strings match empty patterns, but not non-empty ones.
    {u"", {u"", u"^$"}, {u"a"}},
    // Non-empty strings don't match empty patterns.
    {u"a", {u""}, {u"^$"}},
    // Beginning and end of the line and exact matches.
    {u"head_tail",
     {u"^head", u"tail$", u"^head_tail$"},
     {u"head$", u"^tail", u"^head$", u"^tail$"}},
    // Escaped dots.
    // Note: The unescaped "." characters are wild cards.
    {u"m.i.", {u"m.i.", u"m\\.i\\."}},
    {u"mXiX", {u"m.i."}, {u"m\\.i\\."}},
    // Repetition.
    {u"headtail", {u"head.*tail"}, {u"head.+tail"}},
    {u"headXtail", {u"head.*tail", u"head.+tail"}},
    {u"headXXXtail", {u"head.*tail", u"head.+tail"}},
    // Alternation.
    {u"head_tail", {u"head|other", u"tail|other"}, {u"bad|good"}},
    // Case sensitivity.
    {u"xxxHeAd_tAiLxxx", {u"head_tail"}},
    // Word boundaries.
    {u"contains word:", {u"\\bword\\b"}, {u"\\bcon\\b"}},
    // Make sure the circumflex in 'crêpe' is not treated as a word boundary.
    {u"crêpe", {}, {u"\\bcr\\b"}}};

INSTANTIATE_TEST_SUITE_P(FormFieldTest,
                         MatchTest,
                         testing::ValuesIn(kMatchTestCases));

TEST_P(MatchTest, Match) {
  const auto& [label, positive_patterns, negative_patterns] = GetParam();
  constexpr MatchParams kMatchLabel{{MatchAttribute::kLabel}, {}};
  AutofillField field;
  SCOPED_TRACE("label = " + base::UTF16ToUTF8(label));
  SetFieldLabels(&field, label);
  for (const auto& pattern : positive_patterns) {
    SCOPED_TRACE("positive_pattern = " + base::UTF16ToUTF8(pattern));
    EXPECT_TRUE(FormField::MatchForTesting(&field, pattern, kMatchLabel));
  }
  for (const auto& pattern : negative_patterns) {
    SCOPED_TRACE("negative_pattern = " + base::UTF16ToUTF8(pattern));
    EXPECT_FALSE(FormField::MatchForTesting(&field, pattern, kMatchLabel));
  }
}

// Test that we ignore checkable elements.
TEST_P(FormFieldTest, ParseFormFields) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  FormFieldData field_data;
  field_data.form_control_type = "text";

  field_data.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
  field_data.label = u"Is PO Box";
  field_data.unique_renderer_id = MakeFieldRendererId();
  fields.push_back(std::make_unique<AutofillField>(field_data));

  // Does not parse since there are only field and it's checkable.
  // An empty page_language means the language is unknown and patterns of all
  // languages are used.
  EXPECT_TRUE(FormField::ParseFormFields(fields, LanguageCode(""),
                                         /*is_form_tag=*/true, pattern_source(),
                                         /*log_manager=*/nullptr)
                  .empty());

  // reset |is_checkable| to false.
  field_data.check_status = FormFieldData::CheckStatus::kNotCheckable;
  field_data.label = u"Address line1";
  field_data.unique_renderer_id = MakeFieldRendererId();
  fields.push_back(std::make_unique<AutofillField>(field_data));

  // Parse a single address line 1 field.
  ASSERT_EQ(0u,
            FormField::ParseFormFields(fields, LanguageCode(""),
                                       /*is_form_tag=*/true, pattern_source(),
                                       /*log_manager=*/nullptr)
                .size());

  // Parses address line 1 and 2.
  field_data.label = u"Address line2";
  field_data.unique_renderer_id = MakeFieldRendererId();
  fields.push_back(std::make_unique<AutofillField>(field_data));

  // An empty page_language means the language is unknown and patterns of
  // all languages are used.
  ASSERT_EQ(0u,
            FormField::ParseFormFields(fields, LanguageCode(""),
                                       /*is_form_tag=*/true, pattern_source(),
                                       /*log_manager=*/nullptr)
                .size());
}

// Test that the minimum number of required fields for the heuristics considers
// whether a field is actually fillable.
TEST_P(FormFieldTest, ParseFormFieldEnforceMinFillableFields) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  FormFieldData field_data;
  field_data.form_control_type = "text";

  field_data.label = u"Address line 1";
  field_data.unique_renderer_id = MakeFieldRendererId();
  fields.push_back(std::make_unique<AutofillField>(field_data));

  field_data.label = u"Address line 2";
  field_data.unique_renderer_id = MakeFieldRendererId();
  fields.push_back(std::make_unique<AutofillField>(field_data));

  // Don't parse forms with 2 fields.
  // An empty page_language means the language is unknown and patterns of all
  // languages are used.
  EXPECT_EQ(0u,
            FormField::ParseFormFields(fields, LanguageCode(""),
                                       /*is_form_tag=*/true, pattern_source(),
                                       /*log_manager=*/nullptr)
                .size());

  field_data.label = u"Search";
  field_data.unique_renderer_id = MakeFieldRendererId();
  fields.push_back(std::make_unique<AutofillField>(field_data));

  // Before the fix in kAutofillFixFillableFieldTypes, we would parse the form
  // now, although a search field is not fillable.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kAutofillFixFillableFieldTypes);
    // An empty page_language means the language is unknown and patterns of all
    // languages are used.
    EXPECT_EQ(3u, FormField::ParseFormFields(
                      fields, LanguageCode(""), /*is_form_tag=*/true,
                      pattern_source(), /*log_manager=*/nullptr)
                      .size());
  }

  // With the fix, we don't parse the form because search fields are not
  // fillable (therefore, the form has only 2 fillable fields).
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kAutofillFixFillableFieldTypes);
    // An empty page_language means the language is unknown and patterns of all
    // languages are used.
    const FieldCandidatesMap field_candidates_map = FormField::ParseFormFields(
        fields, LanguageCode(""), /*is_form_tag=*/true, pattern_source(),
        /*log_manager=*/nullptr);
    EXPECT_EQ(0u, FormField::ParseFormFields(
                      fields, LanguageCode(""), /*is_form_tag=*/true,
                      pattern_source(), /*log_manager=*/nullptr)
                      .size());
  }
}

// Test that the parseable label is used when the feature is enabled.
TEST_P(FormFieldTest, TestParseableLabels) {
  FormFieldData field_data;
  field_data.form_control_type = "text";

  field_data.label = u"not a parseable label";
  field_data.unique_renderer_id = MakeFieldRendererId();
  auto autofill_field = std::make_unique<AutofillField>(field_data);
  autofill_field->set_parseable_label(u"First Name");

  constexpr MatchParams kMatchLabel{{MatchAttribute::kLabel}, {}};
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kAutofillEnableSupportForParsingWithSharedLabels);
    EXPECT_TRUE(FormField::MatchForTesting(autofill_field.get(), u"First Name",
                                           kMatchLabel));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kAutofillEnableSupportForParsingWithSharedLabels);
    EXPECT_FALSE(FormField::MatchForTesting(autofill_field.get(), u"First Name",
                                            kMatchLabel));
  }
}

// Test that |ParseFormFieldsForPromoCodes| parses single field promo codes.
TEST_P(FormFieldTest, ParseFormFieldsForPromoCodes) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(
      features::kAutofillParseMerchantPromoCodeFields);

  std::vector<std::unique_ptr<AutofillField>> fields;
  FormFieldData field_data;
  field_data.form_control_type = "text";

  // Parse single field promo code.
  field_data.label = u"Promo code";
  field_data.unique_renderer_id = MakeFieldRendererId();
  fields.push_back(std::make_unique<AutofillField>(field_data));

  EXPECT_EQ(
      1u, FormField::ParseFormFieldsForPromoCodes(
              fields, LanguageCode(""), /*is_form_tag=*/true, pattern_source())
              .size());

  // Don't parse other fields.
  field_data.label = u"Address line 1";
  field_data.unique_renderer_id = MakeFieldRendererId();
  fields.push_back(std::make_unique<AutofillField>(field_data));

  // Still only the promo code field should be parsed.
  EXPECT_EQ(
      1u, FormField::ParseFormFieldsForPromoCodes(
              fields, LanguageCode(""), /*is_form_tag=*/true, pattern_source())
              .size());
}

struct ParseInAnyOrderTestcase {
  // An nxn matrix, describing that field i is matched by parser j.
  std::vector<std::vector<bool>> field_matches_parser;
  // The expected order in which the n fields are matched, or empty, if the
  // matching is expected to fail.
  std::vector<int> expected_permutation;
};

class ParseInAnyOrderTest
    : public testing::TestWithParam<ParseInAnyOrderTestcase> {};

const ParseInAnyOrderTestcase kParseInAnyOrderTestcases[]{
    // Field i is only matched by parser i -> matched in order.
    {{{true, false, false}, {false, true, false}, {false, false, true}},
     {0, 1, 2}},
    // Opposite order.
    {{{false, true}, {true, false}}, {1, 0}},
    // The second field has to go first, because it is only matched by the first
    // parser.
    {{{true, true}, {true, false}}, {1, 0}},
    // The second parser doesn't match any field, thus no match.
    {{{true, false}, {true, false}}, {}},
    // No field matches.
    {{{false, false}, {false, false}}, {}}};

INSTANTIATE_TEST_SUITE_P(FormFieldTest,
                         ParseInAnyOrderTest,
                         testing::ValuesIn(kParseInAnyOrderTestcases));

TEST_P(ParseInAnyOrderTest, ParseInAnyOrder) {
  auto testcase = GetParam();
  bool expect_success = !testcase.expected_permutation.empty();
  size_t n = testcase.field_matches_parser.size();

  std::vector<std::unique_ptr<AutofillField>> fields;
  // Creates n fields and encodes their ids in `max_length`, as `id_attribute`
  // is a string.
  for (size_t i = 0; i < n; i++) {
    FormFieldData form_field_data;
    form_field_data.max_length = i;
    fields.push_back(std::make_unique<AutofillField>(form_field_data));
  }

  // Checks if `matching_ids` of the `scanner`'s current position is true.
  // This is used to simulate different parsers, as described by
  // `testcase.field_matches_parser`.
  auto Matches = [](AutofillScanner* scanner,
                    const std::vector<bool>& matching_ids) -> bool {
    return matching_ids[scanner->Cursor()->max_length];
  };

  // Construct n parsers from `testcase.field_matches_parser`.
  AutofillScanner scanner(fields);
  std::vector<AutofillField*> matched_fields(n);
  std::vector<std::pair<AutofillField**, base::RepeatingCallback<bool()>>>
      fields_and_parsers;
  for (size_t i = 0; i < n; i++) {
    fields_and_parsers.emplace_back(
        &matched_fields[i],
        base::BindRepeating(Matches, &scanner,
                            testcase.field_matches_parser[i]));
  }

  EXPECT_EQ(FormField::ParseInAnyOrderForTesting(&scanner, fields_and_parsers),
            expect_success);

  if (expect_success) {
    EXPECT_TRUE(scanner.IsEnd());
    ASSERT_EQ(testcase.expected_permutation.size(), n);
    for (size_t i = 0; i < n; i++) {
      EXPECT_EQ(matched_fields[i],
                fields[testcase.expected_permutation[i]].get());
    }
  } else {
    EXPECT_EQ(scanner.CursorPosition(), 0u);
    EXPECT_THAT(matched_fields, ::testing::Each(nullptr));
  }
}

}  // namespace autofill
