// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_DELEGATE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_DELEGATE_IMPL_H_

#include "base/component_export.h"
#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"

namespace policy {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
    DeviceRestrictionScheduleControllerDelegateImpl
    : public DeviceRestrictionScheduleController::Delegate {
 public:
  // Id of the upcoming logout notification.
  static constexpr char kUpcomingLogoutNotificationId[] =
      "policy.device_restriction_schedule.upcoming_logout";
  // Id of the post-logout notification.
  static constexpr char kPostLogoutNotificationId[] =
      "policy.device_restriction_schedule.post_logout";

  DeviceRestrictionScheduleControllerDelegateImpl();

  // DeviceRestrictionScheduleController::Delegate:
  bool IsUserLoggedIn() const override;
  void ShowUpcomingLogoutNotification(base::Time logout_time) override;
  void ShowPostLogoutNotification() override;
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_DELEGATE_IMPL_H_
