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

TEST(OriginGatingConfigurationTest, CheckFails_NoVerdict) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        OriginGatingConfiguration config({DecisionSource::kNoVerdict},
                                         /*use_site_keyed_cache=*/false);
      },
      "");
}

}  // namespace origin_gating
