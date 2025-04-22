// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_lens_overlay_side_panel_coordinator.h"

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

namespace lens {

TestLensOverlaySidePanelCoordinator::TestLensOverlaySidePanelCoordinator(
    LensOverlayController* lens_overlay_controller)
    : LensOverlaySidePanelCoordinator(lens_overlay_controller) {}

TestLensOverlaySidePanelCoordinator::~TestLensOverlaySidePanelCoordinator() =
    default;

void TestLensOverlaySidePanelCoordinator::SetSidePanelIsLoadingResults(
    bool is_loading) {
  if (is_loading) {
    side_panel_loading_set_to_true_++;
    return;
  }

  side_panel_loading_set_to_false_++;
}

void TestLensOverlaySidePanelCoordinator::ResetSidePanelTracking() {
  side_panel_loading_set_to_true_ = 0;
  side_panel_loading_set_to_false_ = 0;
}

}  // namespace lens
