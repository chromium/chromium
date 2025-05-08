// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_TEST_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_LENS_TEST_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_

#include "lens_overlay_side_panel_coordinator.h"

namespace lens {

// Helper for testing features that use the LensOverlaySidePanelCoordinator.
// The only logic in this class should be for tracking sent request data. Actual
// LensOverlaySidePanelCoordinator logic should not be stubbed out.
class TestLensOverlaySidePanelCoordinator
    : public LensOverlaySidePanelCoordinator {
 public:
  explicit TestLensOverlaySidePanelCoordinator(
      LensSearchController* lens_search_controller);
  ~TestLensOverlaySidePanelCoordinator() override;

  void SetSidePanelIsLoadingResults(bool is_loading) override;

  void ResetSidePanelTracking();

  int side_panel_loading_set_to_true_ = 0;
  int side_panel_loading_set_to_false_ = 0;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_TEST_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_
