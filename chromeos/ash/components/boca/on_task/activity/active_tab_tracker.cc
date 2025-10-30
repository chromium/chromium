// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/activity/active_tab_tracker.h"

#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void UpdateTabActivity(const std::u16string& tab_title) {
  // Fetch dependency on the fly to avoid dangling pointers. Boca app client is
  // guaranteed live throughout boca lifecycle.
  ash::boca::BocaAppClient::Get()->GetSessionManager()->UpdateTabActivity(
      tab_title);
}

}  // namespace

namespace ash::boca {
ActiveTabTracker::ActiveTabTracker() = default;

ActiveTabTracker::~ActiveTabTracker() = default;

void ActiveTabTracker::OnActiveTabChanged(const std::u16string& tab_title) {
  UpdateTabActivity(tab_title);
}

void ActiveTabTracker::OnWindowActivated(const std::u16string& tab_title) {
  UpdateTabActivity(tab_title);
}

void ActiveTabTracker::OnWindowDeactivated() {
  UpdateTabActivity(l10n_util::GetStringUTF16(IDS_NOT_IN_CLASS_TOOLS));
}

}  // namespace ash::boca
