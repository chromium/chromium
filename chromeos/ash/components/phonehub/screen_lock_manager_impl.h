// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_SCREEN_LOCK_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_SCREEN_LOCK_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/screen_lock_manager.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
namespace phonehub {

// Implements ScreenLockManager by persisting the last-known screen lock value
// to user prefs.
class ScreenLockManagerImpl : public ScreenLockManager {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit ScreenLockManagerImpl(PrefService* pref_service);
  ~ScreenLockManagerImpl() override;

 private:
  // ScreenLockManager:
  LockStatus GetLockStatus() const override;
  void SetLockStatusInternal(LockStatus lock_status) override;

  raw_ptr<PrefService> pref_service_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_SCREEN_LOCK_MANAGER_IMPL_H_
