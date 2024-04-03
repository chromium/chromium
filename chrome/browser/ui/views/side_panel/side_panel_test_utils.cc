// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_test_utils.h"

#include <memory>

#include "base/run_loop.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

SidePanelWaiter::SidePanelWaiter(SidePanelCoordinator* side_panel_coordinator)
    : side_panel_coordinator_(side_panel_coordinator) {
  side_panel_coordinator->AddSidePanelViewStateObserver(this);
}

SidePanelWaiter::~SidePanelWaiter() {
  side_panel_coordinator_->RemoveSidePanelViewStateObserver(this);
}

void SidePanelWaiter::WaitForSidePanelClose() {
  if (side_panel_coordinator_->IsSidePanelShowing()) {
    side_panel_close_run_loop_ = std::make_unique<base::RunLoop>();
    side_panel_close_run_loop_->Run();
  }
}

void SidePanelWaiter::OnSidePanelDidClose() {
  if (side_panel_close_run_loop_ && side_panel_close_run_loop_->running()) {
    side_panel_close_run_loop_->Quit();
  }
}
