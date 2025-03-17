// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_SIDE_PANEL_MENU_OPTION_H_
#define COMPONENTS_LENS_LENS_OVERLAY_SIDE_PANEL_MENU_OPTION_H_

namespace lens {

// An enum to represent the options in the side panel more options menu.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensOverlaySidePanelMenuOption)
enum class LensOverlaySidePanelMenuOption {
  // Unknown menu option.
  kUnknown = 0,
  // Menu option to open in new tab.
  kOpenInNewTab = 1,
  // Menu option to open My Activity.
  kMyActivity = 2,
  // Menu option to learn more.
  kLearnMore = 3,
  // Menu option to send feedback.
  kSendFeedback = 4,

  kMaxValue = kSendFeedback
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensOverlaySidePanelMenuOption)
}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_SIDE_PANEL_MENU_OPTION_H_
