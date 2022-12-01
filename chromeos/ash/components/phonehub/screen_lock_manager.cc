// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/screen_lock_manager.h"

namespace ash {
namespace phonehub {

ScreenLockManager::ScreenLockManager() = default;
ScreenLockManager::~ScreenLockManager() = default;

void ScreenLockManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ScreenLockManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ScreenLockManager::NotifyScreenLockChanged() {
  for (auto& observer : observer_list_)
    observer.OnScreenLockChanged();
}
}  // namespace phonehub
}  // namespace ash
