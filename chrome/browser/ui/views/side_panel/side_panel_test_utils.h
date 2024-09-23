// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_TEST_UTILS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_view_state_observer.h"

// A class which waits on various SidePanelEntryObserver events.
class SidePanelWaiter : public SidePanelViewStateObserver {
 public:
  explicit SidePanelWaiter(SidePanelCoordinator* side_panel_coordinator);

  ~SidePanelWaiter() override;
  SidePanelWaiter(const SidePanelWaiter& other) = delete;
  SidePanelWaiter& operator=(const SidePanelWaiter& other) = delete;

  void WaitForSidePanelClose();

 private:
  void OnSidePanelDidClose() override;

  raw_ptr<SidePanelCoordinator> side_panel_coordinator_;
  std::unique_ptr<base::RunLoop> side_panel_close_run_loop_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_TEST_UTILS_H_
