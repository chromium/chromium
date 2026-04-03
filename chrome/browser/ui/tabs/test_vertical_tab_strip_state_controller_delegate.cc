// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/test_vertical_tab_strip_state_controller_delegate.h"

namespace tabs {

TestVerticalTabStripStateControllerDelegate::
    TestVerticalTabStripStateControllerDelegate() = default;
TestVerticalTabStripStateControllerDelegate::
    ~TestVerticalTabStripStateControllerDelegate() = default;

void TestVerticalTabStripStateControllerDelegate::
    SetCollapsedStateUpdatedCallback(
        base::RepeatingCallback<void(bool)> callback) {
  update_state_controller_collapsed_callback_ = std::move(callback);
}

bool TestVerticalTabStripStateControllerDelegate::IsCollapsing() {
  return false;
}

void TestVerticalTabStripStateControllerDelegate::RequestCollapse(
    bool collapse) {
  update_state_controller_collapsed_callback_.Run(collapse);
}

}  // namespace tabs
