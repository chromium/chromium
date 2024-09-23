// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo_specification.h"

#include "base/feature_list.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace user_education {

namespace {
BASE_FEATURE(kTestRotatingPromo,
             "TEST_RotatingPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestAnchorElement);
}  // namespace

TEST(FeaturePromoSpecificationTest, RotatingPromoOverrideFocusOnShow) {
  FeaturePromoSpecification::RotatingPromos promos(
      FeaturePromoSpecification::CreateForToastPromo(
          kTestRotatingPromo, kTestAnchorElement, IDS_CLOSE_PROMO, 0,
          FeaturePromoSpecification::AcceleratorInfo()),
      std::nullopt,
      FeaturePromoSpecification::CreateForToastPromo(
          kTestRotatingPromo, kTestAnchorElement, IDS_CLOSE_PROMO, 0,
          FeaturePromoSpecification::AcceleratorInfo()));

  promos.at(2)->OverrideFocusOnShow(false);

  auto spec = FeaturePromoSpecification::CreateRotatingPromoForTesting(
      kTestRotatingPromo, std::move(promos));

  spec.OverrideFocusOnShow(true);

  EXPECT_EQ(true, spec.rotating_promos().at(0)->focus_on_show_override());
  EXPECT_EQ(std::nullopt, spec.rotating_promos().at(1));
  EXPECT_EQ(false, spec.rotating_promos().at(2)->focus_on_show_override());
}

}  // namespace user_education
