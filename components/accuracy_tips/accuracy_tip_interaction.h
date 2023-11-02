// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_INTERACTION_H_
#define COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_INTERACTION_H_

namespace accuracy_tips {

// Represents the different user interactions with a AccuracyTip dialog.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AccuracyTipInteraction {
  // The UI was closed without interaction. E.g. because the tab navigated
  // to a different site.
  kNoAction = 0,
  // Learn more button pressed.
  kLearnMore = 1,
  // Opt out button pressed.
  kOptOut = 2,
  // Pressed ESC or close button.
  kClosed = 3,
  // Logged when the UI was not actually shown due to experiment
  // configuration.
  kDisabledByExperiment = 4,
  // Pressed "ignore" button.
  kIgnore = 5,
  // The UI was closed because the site requested a permission.
  kPermissionRequested = 6,

  kMaxValue = kPermissionRequested,
};

}  // namespace accuracy_tips

#endif  // COMPONENTS_ACCURACY_TIPS_ACCURACY_TIP_INTERACTION_H_
