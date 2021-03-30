// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/feature_promo_text_replacements.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/data/grit/chrome_test_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(FeaturePromoTextReplacements, EmptyReplacement) {
  EXPECT_EQ(u"Use this feature please",
            FeaturePromoTextReplacements().ApplyTo(IDS_FEATURE_PROMO_BODY));
}

TEST(FeaturePromoTextReplacements, OneStringReplacement) {
  EXPECT_EQ(u"Click the button to do something",
            FeaturePromoTextReplacements::WithString(u"the button")
                .ApplyTo(IDS_FEATURE_PROMO_BODY_WITH_STRING_PLACEHOLDER));
}
