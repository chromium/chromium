// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/activity/active_tab_tracker.h"

#include "chromeos/ash/components/boca/boca_app_client.h"

namespace ash::boca {
ActiveTabTracker::ActiveTabTracker() = default;

ActiveTabTracker::~ActiveTabTracker() = default;

void ActiveTabTracker::OnActiveTabChanged(const std::u16string& tab_title) {
  // Fetch dependency on the fly to avoid dangling pointers. Boca app client is
  // guaranteed live throughout boca lifecycle.
  BocaAppClient::Get()->GetSessionManager()->UpdateTabActivity(tab_title);
}

}  // namespace ash::boca
