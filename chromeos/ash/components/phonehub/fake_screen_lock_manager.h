// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_SCREEN_LOCK_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_SCREEN_LOCK_MANAGER_H_

#include "chromeos/ash/components/phonehub/screen_lock_manager.h"

namespace ash {
namespace phonehub {

// A fake versions of the core business logic of ScreenLockManager.
class FakeScreenLockManager : public ScreenLockManager {
 public:
  explicit FakeScreenLockManager(
      LockStatus lock_status = LockStatus::kLockedOn);
  ~FakeScreenLockManager() override;

  FakeScreenLockManager(const FakeScreenLockManager&) = delete;
  FakeScreenLockManager& operator=(const FakeScreenLockManager&) = delete;

  // ScreenLockManager:
  void SetLockStatusInternal(LockStatus lock_status) override;
  LockStatus GetLockStatus() const override;

 private:
  LockStatus lock_status_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_SCREEN_LOCK_MANAGER_H_
