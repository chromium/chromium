// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/alert/child_tab_alert_helper.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

DEFINE_USER_DATA(ChildTabAlertHelper);

ChildTabAlertHelper::ChildTabAlertHelper(TabInterface& tab)
    : scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

ChildTabAlertHelper::~ChildTabAlertHelper() = default;

// static
ChildTabAlertHelper* ChildTabAlertHelper::From(TabInterface* tab) {
  CHECK(tab);
  return Get(tab->GetUnownedUserDataHost());
}

base::CallbackListSubscription
ChildTabAlertHelper::RegisterChildAlertsStateChange(
    ChildAlertsStateChangeCallback callback) {
  return child_alerts_state_change_callbacks_.Add(std::move(callback));
}

base::ScopedClosureRunner ChildTabAlertHelper::CreateChildMediaAlert(
    TabAlert alert) {
  CHECK(alert == TabAlert::kAudioRecording ||
        alert == TabAlert::kVideoRecording ||
        alert == TabAlert::kMediaRecording);
  if (++child_alert_counts_[alert] == 1u) {
    child_alerts_state_change_callbacks_.Notify();
  }
  return base::ScopedClosureRunner(
      base::BindOnce(&ChildTabAlertHelper::OnChildAlertReleased,
                     weak_factory_.GetWeakPtr(), alert));
}

bool ChildTabAlertHelper::IsChildAlertActive(TabAlert alert) const {
  return child_alert_counts_.contains(alert);
}

void ChildTabAlertHelper::OnChildAlertReleased(TabAlert alert) {
  auto it = child_alert_counts_.find(alert);
  CHECK(it != child_alert_counts_.end());
  CHECK_GT(it->second, 0u);
  if (--it->second == 0u) {
    child_alert_counts_.erase(it);
    child_alerts_state_change_callbacks_.Notify();
  }
}

}  // namespace tabs
