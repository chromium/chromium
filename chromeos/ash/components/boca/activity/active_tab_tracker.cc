// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/activity/active_tab_tracker.h"

#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"

namespace ash::boca {
ActiveTabTracker::ActiveTabTracker() = default;
ActiveTabTracker::~ActiveTabTracker() = default;
void ActiveTabTracker::OnActiveTabChanged(const std::u16string& tab_title) {}
}  // namespace ash::boca
