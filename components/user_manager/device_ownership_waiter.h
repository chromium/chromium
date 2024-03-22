// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_DEVICE_OWNERSHIP_WAITER_H_
#define COMPONENTS_USER_MANAGER_DEVICE_OWNERSHIP_WAITER_H_

#include "base/functional/callback_forward.h"
#include "components/user_manager/user_manager_export.h"

namespace user_manager {

class USER_MANAGER_EXPORT DeviceOwnershipWaiter {
 public:
  virtual ~DeviceOwnershipWaiter() = default;

  // Delays execution of `callback` until the device owner is initialized in
  // `UserManager`. The delay is skipped (and the callback invoked immediately)
  // in the following cases:
  // - this is a guest session: Guest sessions can occur before the initial OOBE
  //   and are by design without an owner.
  // - this is a demo mode session: Same as guest session.
  // - we are running ChromeOS on Linux: The `DeviceSettingsService` is not
  //   behaving as in the real world for these builds, hence we can skip the
  //   check.
  //
  // Furthermore there are some situations that we don't need to special case,
  // although the ownership question might seem tricky:
  // - this is a managed guest session: To start a MGS, devices need
  //   to first go through enterprise enrollment, which is an action that
  //   already establishes device ownership.
  // - this is a kiosk session: Currently kiosk sessions are only allowed after
  //   enterprise enrollment, which similar to MGS, establishes device
  //   ownership.
  //
  // This must be called after login.
  virtual void WaitForOwnershipFetched(base::OnceClosure callback) = 0;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_DEVICE_OWNERSHIP_WAITER_H_
