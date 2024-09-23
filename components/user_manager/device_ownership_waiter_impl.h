// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_DEVICE_OWNERSHIP_WAITER_IMPL_H_
#define COMPONENTS_USER_MANAGER_DEVICE_OWNERSHIP_WAITER_IMPL_H_

#include "base/functional/callback_forward.h"
#include "components/user_manager/device_ownership_waiter.h"
#include "components/user_manager/user_manager_export.h"

namespace user_manager {

// Implementation of DeviceOwnershipWaiter. Waits until the device ownership
// will be fetched.
class USER_MANAGER_EXPORT DeviceOwnershipWaiterImpl
    : public DeviceOwnershipWaiter {
 public:
  DeviceOwnershipWaiterImpl();

  DeviceOwnershipWaiterImpl(const DeviceOwnershipWaiterImpl&) = delete;
  DeviceOwnershipWaiterImpl& operator=(const DeviceOwnershipWaiterImpl&) =
      delete;

  ~DeviceOwnershipWaiterImpl() override;

  // DeviceOwnershipWaiter:
  void WaitForOwnershipFetched(base::OnceClosure callback) override;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_DEVICE_OWNERSHIP_WAITER_IMPL_H_
