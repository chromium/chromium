// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_GMS_CORE_NOTIFICATIONS_STATE_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_GMS_CORE_NOTIFICATIONS_STATE_TRACKER_H_

#include <string>
#include <vector>

#include "chromeos/ash/components/tether/gms_core_notifications_state_tracker.h"

namespace ash {

namespace tether {

// Test double for GmsCoreNotificationsStateTracker.
class FakeGmsCoreNotificationsStateTracker
    : public GmsCoreNotificationsStateTracker {
 public:
  FakeGmsCoreNotificationsStateTracker();

  FakeGmsCoreNotificationsStateTracker(
      const FakeGmsCoreNotificationsStateTracker&) = delete;
  FakeGmsCoreNotificationsStateTracker& operator=(
      const FakeGmsCoreNotificationsStateTracker&) = delete;

  ~FakeGmsCoreNotificationsStateTracker() override;

  void set_device_names(const std::vector<std::string>& device_names) {
    device_names_ = device_names;
  }

  void NotifyGmsCoreNotificationStateChanged();

  // GmsCoreNotificationsStateTracker:
  std::vector<std::string> GetGmsCoreNotificationsDisabledDeviceNames()
      override;

 private:
  std::vector<std::string> device_names_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_GMS_CORE_NOTIFICATIONS_STATE_TRACKER_H_
