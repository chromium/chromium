// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "components/exo/shell_surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/shell_surface_builder.h"
#include "ui/compositor/test/test_utils.h"

namespace exo {
namespace {

using EventLatencyMetricsTest = test::ExoTestBase;

// Ash.EventLatency metrics should not be recorded when the target window
// is an exo windows.
TEST_F(EventLatencyMetricsTest, NoReportForExo) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({100, 100}).SetAsPopup().BuildShellSurface();
  views::Widget* widget = shell_surface->GetWidget();
  ui::Compositor* compositor = widget->GetCompositor();

  base::HistogramTester histogram_tester;
  widget->Activate();
  // Key events are handled by shell surface by default.
  PressAndReleaseKey(ui::VKEY_A);
  compositor->ScheduleFullRedraw();
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(compositor));

  // Event latency metrics should not be recorded if the target window is
  // an exo window.
  histogram_tester.ExpectTotalCount("Ash.EventLatency.KeyPressed.TotalLatency",
                                    0);
  histogram_tester.ExpectTotalCount("Ash.EventLatency.KeyReleased.TotalLatency",
                                    0);
  histogram_tester.ExpectTotalCount("Ash.EventLatency.TotalLatency", 0);
}

}  // namespace
}  // namespace exo
