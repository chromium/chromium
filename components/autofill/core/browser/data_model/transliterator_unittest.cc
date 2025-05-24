// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/transliterator.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class TransliteratorTest : public ::testing::Test {
 public:
  TransliteratorTest() { ClearCachedTransliterators(); }
};

TEST_F(TransliteratorTest, RemoveDiacriticsAndConvertToLowerCase) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(RemoveDiacriticsAndConvertToLowerCase(
                u"āēaa11.īūčģķļņšžKāäǟḑēīļņōȯȱõȭŗšțūžßł"),
            u"aeaa11.iucgklnszkaaadeilnooooorstuzssl");
  EXPECT_EQ(RemoveDiacriticsAndConvertToLowerCase(u"ABC.Ó"), u"abc.o");
  // Check that there is only one transliterator object created.
  histogram_tester.ExpectUniqueSample("Autofill.TransliteratorInitStatus", true,
                                      1);
}

TEST_F(TransliteratorTest, GermanTransliteration) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features{
      features::kAutofillEnableGermanTransliteration};
  EXPECT_EQ(
      RemoveDiacriticsAndConvertToLowerCase(u"ä_ö_ü_ß", AddressCountryCode("")),
      u"a_o_u_ss");
  EXPECT_EQ(RemoveDiacriticsAndConvertToLowerCase(u"ä_ö_ü_ß",
                                                  AddressCountryCode("DE")),
            u"ae_oe_ue_ss");
  EXPECT_EQ(RemoveDiacriticsAndConvertToLowerCase(u"Ä_Ö_Ü_ß",
                                                  AddressCountryCode("DE")),
            u"ae_oe_ue_ss");
  // Check that the transliterator object is initialized only twice (one for
  // the default transliteration without the country code present and the other
  // for the german transliteration).
  histogram_tester.ExpectUniqueSample("Autofill.TransliteratorInitStatus", true,
                                      2);
}

}  // namespace autofill
