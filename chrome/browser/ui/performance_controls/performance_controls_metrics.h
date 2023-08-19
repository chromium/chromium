// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_METRICS_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_METRICS_H_

// Enums for histograms:
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BatterySaverBubbleActionType {
  kTurnOffNow = 0,
  kDismiss = 1,
  kMaxValue = kDismiss
};

enum class HighEfficiencyBubbleActionType {
  kOpenSettings = 0,
  kDismiss = 1,
  kAddException = 2,
  kMaxValue = kAddException
};

enum class HighEfficiencyChipState {
  kCollapsed = 0,
  kExpandedEducation = 1,
  kExpandedWithSavings = 2,
  kMaxValue = kExpandedWithSavings
};
// End of enums for histograms.

void RecordBatterySaverBubbleAction(BatterySaverBubbleActionType type);
void RecordBatterySaverIPHOpenSettings(bool success);
void RecordHighEfficiencyBubbleAction(HighEfficiencyBubbleActionType type);
void RecordHighEfficiencyIPHEnableMode(bool success);
void RecordHighEfficiencyChipState(HighEfficiencyChipState type);

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_METRICS_H_
