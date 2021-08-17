// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_SCREEN_LOCK_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_SCREEN_LOCK_MANAGER_H_

#include "chromeos/components/phonehub/screen_lock_manager.h"

namespace chromeos {
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
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_SCREEN_LOCK_MANAGER_H_
