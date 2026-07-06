// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_configuration.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace origin_gating {

TEST(OriginGatingConfigurationTest, StoresPredicatesInOrder) {
  OriginGatingConfiguration config(
      {
          DecisionSource::kAllowSameOrigin,
      },
      /*use_site_keyed_cache=*/false);
  EXPECT_THAT(config.predicates(),
              testing::ElementsAre(DecisionSource::kAllowSameOrigin));
}

class OriginGatingConfigurationInvalidPredicateTest
    : public ::testing::TestWithParam<DecisionSource> {};

TEST_P(OriginGatingConfigurationInvalidPredicateTest, CheckFails) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        OriginGatingConfiguration config({GetParam()},
                                         /*use_site_keyed_cache=*/false);
      },
      "");
}

INSTANTIATE_TEST_SUITE_P(All,
                         OriginGatingConfigurationInvalidPredicateTest,
                         ::testing::Values(DecisionSource::kNoVerdict));

}  // namespace origin_gating
