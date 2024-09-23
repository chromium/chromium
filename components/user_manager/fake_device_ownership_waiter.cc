// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/fake_device_ownership_waiter.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/user_manager/user_manager.h"

namespace user_manager {

FakeDeviceOwnershipWaiter::FakeDeviceOwnershipWaiter() = default;

FakeDeviceOwnershipWaiter::~FakeDeviceOwnershipWaiter() = default;

void FakeDeviceOwnershipWaiter::WaitForOwnershipFetched(
    base::OnceClosure callback) {
  if (UserManager::Get()->IsLoggedInAsGuest() ||
      (ash::InstallAttributes::IsInitialized() &&
       ash::InstallAttributes::Get()->IsDeviceInDemoMode())) {
    std::move(callback).Run();
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

}  // namespace user_manager
