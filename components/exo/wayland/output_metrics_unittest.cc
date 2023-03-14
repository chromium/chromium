// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/output_metrics.h"

#include "base/bit_cast.h"
#include "components/exo/test/exo_test_base.h"
#include "ui/base/wayland/wayland_display_util.h"

namespace exo::wayland {

using OutputMetricsTest = test::ExoTestBase;

TEST_F(OutputMetricsTest, CorrectlyMapsDisplayStateToOutputMetrics) {
  constexpr float kDisplayScaleFactor = 2;
  constexpr gfx::Point kDisplayOrigin(10, 20);
  constexpr gfx::Size kDisplaySize(800, 600);
  constexpr gfx::Rect kDisplayBounds(kDisplayOrigin, kDisplaySize);
  constexpr gfx::Insets kDisplayInsets(10);
  gfx::Rect display_work_area = kDisplayBounds;
  display_work_area.Inset(kDisplayInsets);

  display::Display display(GetPrimaryDisplay().id(), kDisplayBounds);
  display.set_device_scale_factor(kDisplayScaleFactor);
  display.set_rotation(display::Display::ROTATE_180);
  display.set_panel_rotation(display::Display::ROTATE_270);
  display.set_work_area(display_work_area);
  UpdateDisplay("1600x1200*2");

  OutputMetrics output_metrics(display);

  // wl_output
  EXPECT_EQ(kDisplayOrigin, output_metrics.origin);
  EXPECT_EQ(gfx::Size(1600, 1200), output_metrics.physical_size_px);
  EXPECT_EQ(WL_OUTPUT_TRANSFORM_90, output_metrics.panel_transform);
  EXPECT_EQ(kDisplayScaleFactor, output_metrics.scale);

  // xdg_output
  EXPECT_EQ(kDisplayOrigin, output_metrics.logical_origin);
  EXPECT_EQ(kDisplaySize, output_metrics.logical_size);

  // aura_output
  auto display_id = ui::wayland::FromWaylandDisplayIdPair(
      {output_metrics.display_id.high, output_metrics.display_id.low});
  EXPECT_EQ(GetPrimaryDisplay().id(), display_id);
  EXPECT_FALSE(display.IsInternal());
  EXPECT_EQ(ZAURA_OUTPUT_CONNECTION_TYPE_UNKNOWN,
            output_metrics.connection_type);
  EXPECT_EQ(kDisplayInsets, output_metrics.logical_insets);
  EXPECT_EQ(static_cast<uint32_t>(kDisplayScaleFactor * 1000),
            output_metrics.device_scale_factor_deprecated);
  EXPECT_EQ(kDisplayScaleFactor, output_metrics.device_scale_factor);
  EXPECT_EQ(WL_OUTPUT_TRANSFORM_180, output_metrics.logical_transform);
}

}  // namespace exo::wayland
