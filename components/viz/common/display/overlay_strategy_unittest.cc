// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/display/overlay_strategy.h"

#include <string>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsEmpty;
using testing::UnorderedElementsAre;

namespace viz {

TEST(ParseOverlayStrategiesTest, ParseEmptyList) {
  std::vector<OverlayStrategy> strategies = ParseOverlayStrategies("");
  EXPECT_THAT(strategies, IsEmpty());
}

TEST(ParseOverlayStrategiesTest, ParseFullList) {
  std::vector<OverlayStrategy> strategies =
      ParseOverlayStrategies("single-fullscreen,single-on-top,underlay,cast");

  EXPECT_THAT(strategies, UnorderedElementsAre(OverlayStrategy::kFullscreen,
                                               OverlayStrategy::kSingleOnTop,
                                               OverlayStrategy::kUnderlay,
                                               OverlayStrategy::kUnderlayCast));
}

TEST(ParseOverlayStrategiesTest, BadValue) {
  std::vector<OverlayStrategy> strategies =
      ParseOverlayStrategies("single-fullscreen,bad-value,underlay");

  // The string "bad-value" doesn't correspond to an overlay strategy so it
  // should be skipped.
  EXPECT_THAT(strategies, UnorderedElementsAre(OverlayStrategy::kFullscreen,
                                               OverlayStrategy::kUnderlay));
}

}  // namespace viz
