// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller_delegate_impl.h"

#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"

// TODO(b/345186543, isandrk): Implement.

namespace policy {

DeviceRestrictionScheduleControllerDelegateImpl::
    DeviceRestrictionScheduleControllerDelegateImpl() = default;

void DeviceRestrictionScheduleControllerDelegateImpl::BlockLogin(bool enabled) {
}

bool DeviceRestrictionScheduleControllerDelegateImpl::IsUserLoggedIn() const {
  return false;
}

void DeviceRestrictionScheduleControllerDelegateImpl::
    ShowUpcomingLogoutNotification(base::Time time) {}

void DeviceRestrictionScheduleControllerDelegateImpl::
    ShowPostLogoutNotification() {}

}  // namespace policy
