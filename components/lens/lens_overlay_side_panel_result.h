// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_SIDE_PANEL_RESULT_H_
#define COMPONENTS_LENS_LENS_OVERLAY_SIDE_PANEL_RESULT_H_

namespace lens {

// An enum to represent the result status of side panel loads.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LensOverlaySidePanelResultStatus)
enum class SidePanelResultStatus {
  // The result status is unknown or uninitialized. Should not be logged.
  kUnknown = 0,
  // The result frame was shown in the side panel.
  kResultShown = 1,
  // The error page was shown due to the user being offline when the side panel
  // attempted to load.
  kErrorPageShownOffline = 2,
  // The error page was shown due to the initial full image query failing due to
  // error.
  kErrorPageShownStartQueryError = 3,

  kMaxValue = kErrorPageShownStartQueryError
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensOverlaySidePanelResultStatus)
}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_SIDE_PANEL_RESULT_H_
