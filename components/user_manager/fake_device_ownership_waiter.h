// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_FAKE_DEVICE_OWNERSHIP_WAITER_H_
#define COMPONENTS_USER_MANAGER_FAKE_DEVICE_OWNERSHIP_WAITER_H_

#include "base/functional/callback_forward.h"
#include "components/user_manager/device_ownership_waiter.h"
#include "components/user_manager/user_manager_export.h"

namespace user_manager {

// A `DeviceOwnershipWaiter` for tests that immediately executes the
// callback without actually waiting for the owner.
class USER_MANAGER_EXPORT FakeDeviceOwnershipWaiter : public DeviceOwnershipWaiter {
 public:
  FakeDeviceOwnershipWaiter();

  FakeDeviceOwnershipWaiter(const FakeDeviceOwnershipWaiter&) = delete;
  FakeDeviceOwnershipWaiter& operator=(const FakeDeviceOwnershipWaiter&) =
      delete;

  ~FakeDeviceOwnershipWaiter() override;

  // DeviceOwnershipWaiter:
  void WaitForOwnershipFetched(base::OnceClosure callback) override;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_FAKE_DEVICE_OWNERSHIP_WAITER_H_
