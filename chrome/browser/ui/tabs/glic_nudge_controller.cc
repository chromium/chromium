// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_nudge_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"

namespace tabs {

GlicNudgeController::GlicNudgeController() = default;

GlicNudgeController::~GlicNudgeController() = default;

bool GlicNudgeController::GlicNudgeCriteriaMet() {
  return false;
}

}  // namespace tabs
