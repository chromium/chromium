// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_screen_lock_manager.h"

namespace ash {
namespace phonehub {

FakeScreenLockManager::FakeScreenLockManager(LockStatus lock_status)
    : lock_status_(lock_status) {}

FakeScreenLockManager::~FakeScreenLockManager() = default;

ScreenLockManager::LockStatus FakeScreenLockManager::GetLockStatus() const {
  return lock_status_;
}

void FakeScreenLockManager::SetLockStatusInternal(LockStatus lock_status) {
  if (lock_status_ == lock_status)
    return;

  lock_status_ = lock_status;
  NotifyScreenLockChanged();
}

}  // namespace phonehub
}  // namespace ash
