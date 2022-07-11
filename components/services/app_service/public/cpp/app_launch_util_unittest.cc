// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_launch_util.h"

#include "components/services/app_service/public/mojom/types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace apps {

using AppLaunchUtilTest = testing::Test;

TEST_F(AppLaunchUtilTest, ConvertLaunchSource) {
  EXPECT_EQ(LaunchSource::kFromAppListGrid,
            ConvertMojomLaunchSourceToLaunchSource(
                ConvertLaunchSourceToMojomLaunchSource(
                    LaunchSource::kFromAppListGrid)));
  EXPECT_EQ(LaunchSource::kFromUrlHandler,
            ConvertMojomLaunchSourceToLaunchSource(
                ConvertLaunchSourceToMojomLaunchSource(
                    LaunchSource::kFromUrlHandler)));
}

TEST_F(AppLaunchUtilTest, ConvertEmptyWindowInfo) {
  auto mojom_window_info = apps::mojom::WindowInfo::New();
  EXPECT_EQ(mojom_window_info,
            ConvertWindowInfoToMojomWindowInfo(
                ConvertMojomWindowInfoToWindowInfo(mojom_window_info)));
}

TEST_F(AppLaunchUtilTest, ConvertWindowInfo) {
  auto mojom_window_info = apps::mojom::WindowInfo::New();
  mojom_window_info->window_id = 1;
  mojom_window_info->state = 2;
  mojom_window_info->display_id = 3;

  auto mojom_rect = apps::mojom::Rect::New();
  mojom_rect->x = 100;
  mojom_rect->y = 200;
  mojom_rect->width = 300;
  mojom_rect->height = 400;
  mojom_window_info->bounds = std::move(mojom_rect);

  auto window_info = ConvertWindowInfoToMojomWindowInfo(
      ConvertMojomWindowInfoToWindowInfo(mojom_window_info));
  EXPECT_EQ(mojom_window_info,
            ConvertWindowInfoToMojomWindowInfo(
                ConvertMojomWindowInfoToWindowInfo(mojom_window_info)));
}

}  // namespace apps
