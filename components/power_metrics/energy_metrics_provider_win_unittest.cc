// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_metrics/energy_metrics_provider_win.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace power_metrics {

TEST(EnergyMetricsProviderWinTest, CaptureMetrics) {
  auto provider = EnergyMetricsProviderWin::Create();
  ASSERT_NE(provider, nullptr);
  provider->CaptureMetrics();
}

}  // namespace power_metrics
