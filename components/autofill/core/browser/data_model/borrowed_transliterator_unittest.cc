// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(BorrowedTransliterator, RemoveDiacriticsAndConvertToLowerCase) {
  EXPECT_EQ(RemoveDiacriticsAndConvertToLowerCase(
                u"āēaa11.īūčģķļņšžKāäǟḑēīļņōȯȱõȭŗšțūžßł"),
            u"aeaa11.iucgklnszkaaadeilnooooorstuzssl");
}

TEST(BorrowedTransliterator, GermanTransliteration) {
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
}

}  // namespace autofill
