// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_state_controller.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_service.h"

QuickAnswersStateController::QuickAnswersStateController()
    : session_observer_(this) {
  // Register pref changes if use session already started.
  if (ash::Shell::Get()->session_controller() &&
      ash::Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
    PrefService* prefs =
        ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService();
    DCHECK(prefs);
    state_.RegisterPrefChanges(prefs);
  }
}

QuickAnswersStateController::~QuickAnswersStateController() = default;

void QuickAnswersStateController::OnFirstSessionStarted() {
  PrefService* prefs =
      ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  state_.RegisterPrefChanges(prefs);
}
