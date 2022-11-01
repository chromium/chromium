// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/heuristics/commerce_heuristics_provider.h"
#include "base/test/scoped_feature_list.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commerce {
namespace {

const char* kAddToCartButtons[] = {
    "Add to cart",
    "Add 1 item to Cart",
    "Add nike shoes to Cart",
    "Add to Bag",
    "Add all 4 item to Cart",
    "Add for shipping",
    "Add",
    "Buy Now",
};

const char* kNonAddToCartButtons[] = {"Checkout", "Add up", "Move to bag",
                                      "Move to cart", "Add to wishlist"};

class CommerceHeuristicsProviderTest : public testing::Test {
 public:
  void TearDown() override { features_.Reset(); }

 protected:
  base::test::ScopedFeatureList features_;
};

TEST_F(CommerceHeuristicsProviderTest, TestIsAddToCartButtonSpec) {
  features_.InitWithFeaturesAndParameters(
      {{kChromeCartDomBasedHeuristics,
        {{"add-to-cart-button-width", "100"},
         {"add-to-cart-button-height", "50"}}}},
      {});

  EXPECT_TRUE(commerce_heuristics::IsAddToCartButtonSpec(50, 100));
  // Wrong shape because height > width.
  EXPECT_FALSE(commerce_heuristics::IsAddToCartButtonSpec(10, 5));
  // Too wide.
  EXPECT_FALSE(commerce_heuristics::IsAddToCartButtonSpec(50, 120));
  // Too tall.
  EXPECT_FALSE(commerce_heuristics::IsAddToCartButtonSpec(60, 100));
}

TEST_F(CommerceHeuristicsProviderTest, TestIsAddToCartButtonTag) {
  features_.InitAndEnableFeature(kChromeCartDomBasedHeuristics);

  EXPECT_TRUE(commerce_heuristics::IsAddToCartButtonTag("BUTTON"));
  EXPECT_TRUE(commerce_heuristics::IsAddToCartButtonTag("INPUT"));
  EXPECT_TRUE(commerce_heuristics::IsAddToCartButtonTag("A"));
  EXPECT_TRUE(commerce_heuristics::IsAddToCartButtonTag("SPAN"));

  EXPECT_FALSE(commerce_heuristics::IsAddToCartButtonTag("DIV"));
  EXPECT_FALSE(commerce_heuristics::IsAddToCartButtonTag("S"));
}

TEST_F(CommerceHeuristicsProviderTest, TestIsAddToCartButtonText) {
  features_.InitAndEnableFeature(kChromeCartDomBasedHeuristics);

  for (auto* str : kAddToCartButtons) {
    EXPECT_TRUE(commerce_heuristics::IsAddToCartButtonText(str));
  }

  for (auto* str : kNonAddToCartButtons) {
    EXPECT_FALSE(commerce_heuristics::IsAddToCartButtonText(str));
  }
}

}  // namespace
}  // namespace commerce
