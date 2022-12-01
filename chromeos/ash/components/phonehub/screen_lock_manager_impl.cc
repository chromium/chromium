// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/screen_lock_manager_impl.h"

#include "chromeos/ash/components/phonehub/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace phonehub {

// static
void ScreenLockManagerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kScreenLockStatus,
                                static_cast<int>(LockStatus::kUnknown));
}

ScreenLockManagerImpl::ScreenLockManagerImpl(PrefService* pref_service)
    : pref_service_(pref_service) {}

ScreenLockManagerImpl::~ScreenLockManagerImpl() = default;

ScreenLockManager::LockStatus ScreenLockManagerImpl::GetLockStatus() const {
  int status = pref_service_->GetInteger(prefs::kScreenLockStatus);
  return static_cast<LockStatus>(status);
}

void ScreenLockManagerImpl::SetLockStatusInternal(LockStatus lock_status) {
  pref_service_->SetInteger(prefs::kScreenLockStatus,
                            static_cast<int>(lock_status));
  NotifyScreenLockChanged();
}

}  // namespace phonehub
}  // namespace ash
