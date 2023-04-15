// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"

#include "base/metrics/histogram_functions.h"

void RecordBatterySaverBubbleAction(BatterySaverBubbleActionType type) {
  base::UmaHistogramEnumeration("PerformanceControls.BatterySaver.BubbleAction",
                                type);
}

void RecordBatterySaverIPHOpenSettings(bool success) {
  base::UmaHistogramBoolean("PerformanceControls.BatterySaver.IPHOpenSettings",
                            success);
}

void RecordHighEfficiencyBubbleAction(HighEfficiencyBubbleActionType type) {
  base::UmaHistogramEnumeration(
      "PerformanceControls.HighEfficiency.BubbleAction", type);
}

void RecordHighEfficiencyIPHEnableMode(bool success) {
  base::UmaHistogramBoolean("PerformanceControls.HighEfficiency.IPHEnableMode",
                            success);
}
