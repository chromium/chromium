// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar_controller_util.h"

bool ToolbarControllerUtil::prevent_overflow_for_testing_ = false;

// static
void ToolbarControllerUtil::SetPreventOverflowForTesting(
    bool prevent_overflow) {
  prevent_overflow_for_testing_ = prevent_overflow;
}

// static
bool ToolbarControllerUtil::PreventOverflow() {
  return prevent_overflow_for_testing_;
}
