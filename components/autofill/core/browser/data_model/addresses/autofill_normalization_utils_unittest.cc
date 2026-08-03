// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_utils.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::normalization {

TEST(AutofillNormalizationUtilsTest, NormalizeForComparison) {
  EXPECT_EQ(NormalizeForComparison(u"Timothé"), u"timothe");
  EXPECT_EQ(NormalizeForComparison(u" sven-åke "), u"sven ake");
  EXPECT_EQ(NormalizeForComparison(u"Ç 㸐"), u"c 㸐");
  EXPECT_EQ(NormalizeForComparison(u"90210-3214", WhitespaceSpec::kDiscard),
            u"902103214");
  EXPECT_EQ(NormalizeForComparison(u"Timothé-Noël Étienne Périer"),
            u"timothe noel etienne perier");
  EXPECT_EQ(NormalizeForComparison(u"Straße"), u"strasse");
  // NOP.
  EXPECT_EQ(std::u16string(), NormalizeForComparison(std::u16string()));

  // Simple punctuation removed.
  EXPECT_EQ(NormalizeForComparison(u"1600 Amphitheatre, Pkwy."),
            u"1600 amphitheatre pkwy");

  // Unicode punctuation (hyphen and space), multiple spaces collapsed.
  EXPECT_EQ(NormalizeForComparison(u"Mid\x2013Island\x2003 Plaza"),
            u"mid island plaza");

  // Newline character removed.
  EXPECT_EQ(NormalizeForComparison(u"1600 amphitheatre pkwy \n App. 2"),
            u"1600 amphitheatre pkwy app 2");

  // Diacritics removed.
  EXPECT_EQ(NormalizeForComparison(u"まéÖä정"), u"まeoa정");

  // Spaces removed.
  EXPECT_EQ(NormalizeForComparison(u"유 재석", WhitespaceSpec::kDiscard),
            u"유재석");

  // Punctuation removed, Japanese kana normalized.
  EXPECT_EQ(NormalizeForComparison(u"ビル・ゲイツ", WhitespaceSpec::kDiscard),
            u"ヒルケイツ");
}

TEST(AutofillNormalizationUtilsTest,
     NormalizeForComparisonWithGermanTransliteration) {
  base::test::ScopedFeatureList features{
      features::kAutofillEnableGermanTransliteration};
  EXPECT_EQ(NormalizeForComparison(u"Hänsel Str.", WhitespaceSpec::kRetain,
                                   AddressCountryCode("DE")),
            u"haensel str");
  EXPECT_EQ(NormalizeForComparison(u"Hänsel Str.", WhitespaceSpec::kRetain,
                                   AddressCountryCode("US")),
            u"hansel str");
}

TEST(AutofillNormalizationUtilsTest, NormalizeForComparisonWithGlobalRules) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillIntroduceGlobalEmptyValueRewriterRules);

  EXPECT_EQ(NormalizeForComparison(u"null"), u"");
  EXPECT_EQ(NormalizeForComparison(u"none"), u"");
  EXPECT_EQ(NormalizeForComparison(u"nan"), u"");
  EXPECT_EQ(NormalizeForComparison(u"undefined"), u"");
  EXPECT_EQ(NormalizeForComparison(u"not applicable"), u"");
  EXPECT_EQ(NormalizeForComparison(u"n a"), u"");
  EXPECT_EQ(NormalizeForComparison(u"N/A"), u"");
  EXPECT_EQ(NormalizeForComparison(u"null, none, nan"), u"");
  EXPECT_EQ(NormalizeForComparison(u"(NULL)-NULL-NULL"), u"");
  EXPECT_EQ(NormalizeForComparison(u"123 Main St null"), u"123 main st");
  EXPECT_EQ(NormalizeForComparison(u"null 123 Main St"), u"123 main st");
  EXPECT_EQ(NormalizeForComparison(u"123 Main null St"), u"123 main st");

  // Ensure it doesn't remove parts of words.
  EXPECT_EQ(NormalizeForComparison(u"banana"), u"banana");
  EXPECT_EQ(NormalizeForComparison(u"nonevent"), u"nonevent");
}

TEST(AutofillNormalizationUtilsTest,
     NormalizeForComparisonWithCountrySpecificRules) {
  EXPECT_EQ(NormalizeForComparison(u"unit #3", WhitespaceSpec::kRetain,
                                   AddressCountryCode("us"),
                                   /*apply_country_rewriter_rules=*/true),
            u"u 3");
  EXPECT_EQ(NormalizeForComparison(u"california", WhitespaceSpec::kRetain,
                                   AddressCountryCode("us"),
                                   /*apply_country_rewriter_rules=*/true),
            u"ca");
}

}  // namespace autofill::normalization
