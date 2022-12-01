// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_SCREEN_LOCK_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_SCREEN_LOCK_MANAGER_H_

#include "base/observer_list.h"

namespace ash {
namespace phonehub {

// Tracks the status of whether the user has enabled screen lock on their phone.
class ScreenLockManager {
 public:
  // Status of screen lock. Numerical values are stored in prefs and should not
  // be changed or reused.
  enum class LockStatus {
    // Default screen lock status when the values is not synced from the phone
    // yet.
    kUnknown = 0,
    // Screen lock is not enabled.
    kLockedOff = 1,
    // Screen lock is enabled.
    kLockedOn = 2
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    // Called when screen lock has changed; use GetLockStatus()
    // for the new status.
    virtual void OnScreenLockChanged() = 0;
  };

  ScreenLockManager(const ScreenLockManager&) = delete;
  ScreenLockManager& operator=(const ScreenLockManager&) = delete;
  virtual ~ScreenLockManager();

  virtual LockStatus GetLockStatus() const = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ScreenLockManager();

  void NotifyScreenLockChanged();

 private:
  friend class PhoneStatusProcessor;
  friend class ScreenLockManagerImplTest;

  virtual void SetLockStatusInternal(LockStatus lock_status) = 0;

  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_SCREEN_LOCK_MANAGER_H_
