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

}  // namespace autofill::normalization
