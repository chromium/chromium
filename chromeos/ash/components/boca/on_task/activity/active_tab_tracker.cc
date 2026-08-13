// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/activity/active_tab_tracker.h"

#include "base/check_deref.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"

namespace ash::boca {

ActiveTabTracker::ActiveTabTracker(BocaSessionManager* boca_session_manager)
    : boca_session_manager_(CHECK_DEREF(boca_session_manager)) {}

ActiveTabTracker::~ActiveTabTracker() = default;

void ActiveTabTracker::OnActiveTabChanged(const std::u16string& tab_title) {
  boca_session_manager_->UpdateTabActivity(tab_title);
}

}  // namespace ash::boca
