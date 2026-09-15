// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/mock_vertical_tab_strip_state_controller.h"

namespace tabs::test {

MockVerticalTabStripStateController::MockVerticalTabStripStateController(
    BrowserWindowInterface& browser)
    : VerticalTabStripStateController(browser) {}
MockVerticalTabStripStateController::~MockVerticalTabStripStateController() =
    default;

}  // namespace tabs::test
