// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::features::kAutofillEnforceMinRequiredFieldsForHeuristics;
using base::ASCIIToUTF16;

namespace autofill {

TEST(FormFieldTest, Match) {
  AutofillField field;

  // Empty strings match.
  EXPECT_TRUE(
      FormField::Match(&field, base::string16(), FormField::MATCH_LABEL));

  // Empty pattern matches non-empty string.
  field.label = ASCIIToUTF16("a");
  EXPECT_TRUE(
      FormField::Match(&field, base::string16(), FormField::MATCH_LABEL));

  // Strictly empty pattern matches empty string.
  field.label = base::string16();
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("^$"), FormField::MATCH_LABEL));

  // Strictly empty pattern does not match non-empty string.
  field.label = ASCIIToUTF16("a");
  EXPECT_FALSE(
      FormField::Match(&field, ASCIIToUTF16("^$"), FormField::MATCH_LABEL));

  // Non-empty pattern doesn't match empty string.
  field.label = base::string16();
  EXPECT_FALSE(
      FormField::Match(&field, ASCIIToUTF16("a"), FormField::MATCH_LABEL));

  // Beginning of line.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("^head"), FormField::MATCH_LABEL));
  EXPECT_FALSE(
      FormField::Match(&field, ASCIIToUTF16("^tail"), FormField::MATCH_LABEL));

  // End of line.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_FALSE(
      FormField::Match(&field, ASCIIToUTF16("head$"), FormField::MATCH_LABEL));
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("tail$"), FormField::MATCH_LABEL));

  // Exact.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_FALSE(
      FormField::Match(&field, ASCIIToUTF16("^head$"), FormField::MATCH_LABEL));
  EXPECT_FALSE(
      FormField::Match(&field, ASCIIToUTF16("^tail$"), FormField::MATCH_LABEL));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("^head_tail$"),
                               FormField::MATCH_LABEL));

  // Escaped dots.
  field.label = ASCIIToUTF16("m.i.");
  // Note: This pattern is misleading as the "." characters are wild cards.
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("m.i."), FormField::MATCH_LABEL));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("m\\.i\\."),
                               FormField::MATCH_LABEL));
  field.label = ASCIIToUTF16("mXiX");
  EXPECT_TRUE(
      FormField::Match(&field, ASCIIToUTF16("m.i."), FormField::MATCH_LABEL));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("m\\.i\\."),
                                FormField::MATCH_LABEL));

  // Repetition.
  field.label = ASCIIToUTF16("headtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.*tail"),
                               FormField::MATCH_LABEL));
  field.label = ASCIIToUTF16("headXtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.*tail"),
                               FormField::MATCH_LABEL));
  field.label = ASCIIToUTF16("headXXXtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.*tail"),
                               FormField::MATCH_LABEL));
  field.label = ASCIIToUTF16("headtail");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("head.+tail"),
                                FormField::MATCH_LABEL));
  field.label = ASCIIToUTF16("headXtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.+tail"),
                               FormField::MATCH_LABEL));
  field.label = ASCIIToUTF16("headXXXtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.+tail"),
                               FormField::MATCH_LABEL));

  // Alternation.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head|other"),
                               FormField::MATCH_LABEL));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("tail|other"),
                               FormField::MATCH_LABEL));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("bad|good"),
                                FormField::MATCH_LABEL));

  // Case sensitivity.
  field.label = ASCIIToUTF16("xxxHeAd_tAiLxxx");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head_tail"),
                               FormField::MATCH_LABEL));

  // Word boundaries.
  field.label = ASCIIToUTF16("contains word:");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("\\bword\\b"),
                               FormField::MATCH_LABEL));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("\\bcon\\b"),
                                FormField::MATCH_LABEL));
  // Make sure the circumflex in 'crepe' is not treated as a word boundary.
  field.label = base::UTF8ToUTF16("cr\xC3\xAApe");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("\\bcr\\b"),
                                FormField::MATCH_LABEL));
}

// Test that we ignore checkable elements.
TEST(FormFieldTest, ParseFormFields) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  FormFieldData field_data;
  field_data.form_control_type = "text";

  field_data.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
  field_data.label = ASCIIToUTF16("Is PO Box");
  fields.push_back(
      std::make_unique<AutofillField>(field_data, field_data.label));

  // Does not parse since there are only field and it's checkable.
  EXPECT_TRUE(FormField::ParseFormFields(fields, true).empty());

  // reset |is_checkable| to false.
  field_data.check_status = FormFieldData::CheckStatus::kNotCheckable;

  field_data.label = ASCIIToUTF16("Address line1");
  fields.push_back(
      std::make_unique<AutofillField>(field_data, field_data.label));

  // Parse a single address line 1 field.
  {
    base::test::ScopedFeatureList enforce_min_fields;
    enforce_min_fields.InitAndEnableFeature(
        kAutofillEnforceMinRequiredFieldsForHeuristics);
    ASSERT_EQ(0u, FormField::ParseFormFields(fields, true).size());
  }
  {
    base::test::ScopedFeatureList do_not_enforce_min_fields;
    do_not_enforce_min_fields.InitAndDisableFeature(
        kAutofillEnforceMinRequiredFieldsForHeuristics);
    const FieldCandidatesMap field_candidates_map =
        FormField::ParseFormFields(fields, true);
    ASSERT_EQ(1u, field_candidates_map.size());
    EXPECT_EQ(ADDRESS_HOME_LINE1,
              field_candidates_map.find(ASCIIToUTF16("Address line1"))
                  ->second.BestHeuristicType());
  }

  // Parses address line 1 and 2.
  field_data.label = ASCIIToUTF16("Address line2");
  fields.push_back(
      std::make_unique<AutofillField>(field_data, field_data.label));

  {
    base::test::ScopedFeatureList enforce_min_fields;
    enforce_min_fields.InitAndEnableFeature(
        kAutofillEnforceMinRequiredFieldsForHeuristics);
    ASSERT_EQ(0u, FormField::ParseFormFields(fields, true).size());
  }
  {
    base::test::ScopedFeatureList do_not_enforce_min_fields;
    do_not_enforce_min_fields.InitAndDisableFeature(
        kAutofillEnforceMinRequiredFieldsForHeuristics);
    const FieldCandidatesMap field_candidates_map =
        FormField::ParseFormFields(fields, true);
    ASSERT_EQ(2u, field_candidates_map.size());
    EXPECT_EQ(ADDRESS_HOME_LINE1,
              field_candidates_map.find(ASCIIToUTF16("Address line1"))
                  ->second.BestHeuristicType());
    EXPECT_EQ(ADDRESS_HOME_LINE2,
              field_candidates_map.find(ASCIIToUTF16("Address line2"))
                  ->second.BestHeuristicType());
  }
}

}  // namespace autofill
