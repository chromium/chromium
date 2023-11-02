// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_METRICS_SADTAB_METRICS_TYPES_H_
#define COMPONENTS_UI_METRICS_SADTAB_METRICS_TYPES_H_

namespace ui_metrics {
// An enum for reporting interaction events to a UMA histogram.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.ui_metrics
enum class SadTabEvent {
  // Records that the Sad Tab was displayed.
  DISPLAYED = 0,
  // Records that the main Sad Tab button was triggered.
  BUTTON_CLICKED = 1,
  // Records that the Sad Tab help link was triggered.
  HELP_LINK_CLICKED = 2,
  // Enum end marker.
  MAX_SAD_TAB_EVENT = 3,
};

// Describes the mode of the Sad Tab as being in 'reload' mode.
const char kSadTabReloadHistogramKey[] = "Tabs.SadTab.Reload.Event";
// Describes the mode of the Sad Tab as being in 'feedback' mode.
const char kSadTabFeedbackHistogramKey[] = "Tabs.SadTab.Feedback.Event";
}

#endif  // COMPONENTS_UI_METRICS_SADTAB_METRICS_TYPES_H_
