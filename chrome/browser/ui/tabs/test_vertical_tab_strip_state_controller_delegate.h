// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TEST_VERTICAL_TAB_STRIP_STATE_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_TEST_VERTICAL_TAB_STRIP_STATE_CONTROLLER_DELEGATE_H_

#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"

namespace tabs {

// An implementation of VerticalTabStripStateController::Delegate that
// synchronously updates the collapsed state when collapse is requested,
// suitable for unit tests.
class TestVerticalTabStripStateControllerDelegate final
    : public VerticalTabStripStateController::Delegate {
 public:
  TestVerticalTabStripStateControllerDelegate();
  ~TestVerticalTabStripStateControllerDelegate();

  void SetCollapsedStateUpdatedCallback(
      base::RepeatingCallback<void(bool)> callback) override;
  bool IsCollapsing() override;
  void RequestCollapse(
      tabs::VerticalTabStripState requested_collapse_state) override;

 private:
  base::RepeatingCallback<void(bool)>
      update_state_controller_collapsed_callback_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TEST_VERTICAL_TAB_STRIP_STATE_CONTROLLER_DELEGATE_H_
