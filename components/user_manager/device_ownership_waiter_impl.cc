// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/device_ownership_waiter_impl.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"

namespace user_manager {

DeviceOwnershipWaiterImpl::DeviceOwnershipWaiterImpl() = default;

DeviceOwnershipWaiterImpl::~DeviceOwnershipWaiterImpl() = default;

void DeviceOwnershipWaiterImpl::WaitForOwnershipFetched(
    base::OnceClosure callback) {
  if (UserManager::Get()->IsLoggedInAsGuest() ||
      (ash::InstallAttributes::IsInitialized() &&
       ash::InstallAttributes::Get()->IsDeviceInDemoMode()) ||
      !base::SysInfo::IsRunningOnChromeOS()) {
    std::move(callback).Run();
    return;
  }

  // We assume that there are no kiosk sessions in consumer setups, for more
  // information see docs of this method.
  // TODO(elkurin): Reintroduce the check to assert the device is not in an
  // unmanaged kiosk session.

  UserManager::Get()->GetOwnerAccountIdAsync(
      base::IgnoreArgs<const AccountId&>(std::move(callback)));
}

}  // namespace user_manager
