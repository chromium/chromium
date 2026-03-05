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
  EXPECT_EQ(u"timothe", NormalizeForComparison(u"Timothé"));
  EXPECT_EQ(u"sven ake", NormalizeForComparison(u" sven-åke "));
  EXPECT_EQ(u"c 㸐", NormalizeForComparison(u"Ç 㸐"));
  EXPECT_EQ(u"902103214",
            NormalizeForComparison(u"90210-3214", WhitespaceSpec::kDiscard));
  EXPECT_EQ(u"timothe noel etienne perier",
            NormalizeForComparison(u"Timothé-Noël Étienne Périer"));
  EXPECT_EQ(u"strasse", NormalizeForComparison(u"Straße"));
  // NOP.
  EXPECT_EQ(std::u16string(), NormalizeForComparison(std::u16string()));

  // Simple punctuation removed.
  EXPECT_EQ(u"1600 amphitheatre pkwy",
            NormalizeForComparison(u"1600 Amphitheatre, Pkwy."));

  // Unicode punctuation (hyphen and space), multiple spaces collapsed.
  EXPECT_EQ(u"mid island plaza",
            NormalizeForComparison(u"Mid\x2013Island\x2003 Plaza"));

  // Newline character removed.
  EXPECT_EQ(u"1600 amphitheatre pkwy app 2",
            NormalizeForComparison(u"1600 amphitheatre pkwy \n App. 2"));

  // Diacritics removed.
  EXPECT_EQ(u"まeoa정", NormalizeForComparison(u"まéÖä정"));

  // Spaces removed.
  EXPECT_EQ(u"유재석",
            NormalizeForComparison(u"유 재석", WhitespaceSpec::kDiscard));

  // Punctuation removed, Japanese kana normalized.
  EXPECT_EQ(u"ヒルケイツ",
            NormalizeForComparison(u"ビル・ゲイツ", WhitespaceSpec::kDiscard));
}

TEST(AutofillNormalizationUtilsTest,
     NormalizeForComparisonWithGermanTransliteration) {
  base::test::ScopedFeatureList features{
      features::kAutofillEnableGermanTransliteration};
  EXPECT_EQ(u"haensel str",
            NormalizeForComparison(u"Hänsel Str.", WhitespaceSpec::kRetain,
                                   AddressCountryCode("DE")));
  EXPECT_EQ(u"hansel str",
            NormalizeForComparison(u"Hänsel Str.", WhitespaceSpec::kRetain,
                                   AddressCountryCode("US")));
}

TEST(AutofillNormalizationUtilsTest, NormalizeForComparisonWithGlobalRules) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillIntroduceGlobalEmptyValueRewriterRules);

  EXPECT_EQ(u"", NormalizeForComparison(u"null"));
  EXPECT_EQ(u"", NormalizeForComparison(u"none"));
  EXPECT_EQ(u"", NormalizeForComparison(u"nan"));
  EXPECT_EQ(u"", NormalizeForComparison(u"undefined"));
  EXPECT_EQ(u"", NormalizeForComparison(u"not applicable"));
  EXPECT_EQ(u"", NormalizeForComparison(u"n a"));
  EXPECT_EQ(u"", NormalizeForComparison(u"N/A"));
  EXPECT_EQ(u"", NormalizeForComparison(u"null, none, nan"));
  EXPECT_EQ(u"", NormalizeForComparison(u"(NULL)-NULL-NULL"));
  EXPECT_EQ(u"123 main st", NormalizeForComparison(u"123 Main St null"));
  EXPECT_EQ(u"123 main st", NormalizeForComparison(u"null 123 Main St"));
  EXPECT_EQ(u"123 main st", NormalizeForComparison(u"123 Main null St"));

  // Ensure it doesn't remove parts of words.
  EXPECT_EQ(u"banana", NormalizeForComparison(u"banana"));
  EXPECT_EQ(u"nonevent", NormalizeForComparison(u"nonevent"));
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
