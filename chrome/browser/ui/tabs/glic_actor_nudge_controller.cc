// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_nudge_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace tabs {

DEFINE_USER_DATA(GlicActorNudgeController);
GlicActorNudgeController::GlicActorNudgeController(
    BrowserWindowInterface* browser)
    : scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {}

GlicActorNudgeController::~GlicActorNudgeController() = default;

// static
GlicActorNudgeController* GlicActorNudgeController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

void GlicActorNudgeController::OnStateUpdate() {
  // TODO(crbug.com/431015299): Implement state updates for the Gemini and Actor
  // nudges.
}

}  // namespace tabs
