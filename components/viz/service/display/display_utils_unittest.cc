// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

TEST(DisplayUtilsTest, IsScrollOneElement) {
  ui::LatencyInfo latency_info;
  EXPECT_FALSE(IsScroll({latency_info}));

  ui::LatencyInfo latency_info_scroll_update;
  latency_info_scroll_update.AddLatencyNumber(
      ui::LatencyComponentType::
          INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT);
  EXPECT_TRUE(IsScroll({latency_info_scroll_update}));

  ui::LatencyInfo latency_info_first_scroll;
  latency_info_first_scroll.AddLatencyNumber(
      ui::LatencyComponentType::
          INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT);
  EXPECT_TRUE(IsScroll({latency_info_first_scroll}));
}

TEST(DisplayUtilsTest, IsScroll) {
  ui::LatencyInfo latency_info;

  ui::LatencyInfo latency_info_scroll_update;
  latency_info_scroll_update.AddLatencyNumber(
      ui::LatencyComponentType::
          INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT);

  ui::LatencyInfo latency_info_first_scroll;
  latency_info_first_scroll.AddLatencyNumber(
      ui::LatencyComponentType::
          INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT);

  EXPECT_TRUE(IsScroll(
      {latency_info, latency_info_scroll_update, latency_info_first_scroll}));
  EXPECT_TRUE(IsScroll({latency_info, latency_info_first_scroll}));
  EXPECT_TRUE(IsScroll({latency_info, latency_info_scroll_update}));
}

}  // namespace viz
