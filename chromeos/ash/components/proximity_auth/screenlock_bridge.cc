// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"

#include <string>
#include <utility>

#include <memory>

#include "base/check_is_test.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace proximity_auth {
namespace {

base::LazyInstance<ScreenlockBridge>::DestructorAtExit
    g_screenlock_bridge_instance = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ScreenlockBridge* ScreenlockBridge::Get() {
  return g_screenlock_bridge_instance.Pointer();
}

void ScreenlockBridge::SetLockHandler(LockHandler* lock_handler) {
  // Don't notify observers if there is no change -- i.e. if the screen was
  // already unlocked, and is remaining unlocked.
  if (lock_handler == lock_handler_) {
    return;
  }

  DCHECK(lock_handler_ == nullptr || lock_handler == nullptr);

  lock_handler_ = lock_handler;
  if (lock_handler_) {
    for (auto& observer : observers_) {
      observer.OnScreenDidLock();
    }
  } else {
    focused_account_id_ = EmptyAccountId();
    for (auto& observer : observers_) {
      observer.OnScreenDidUnlock();
    }
  }
}

void ScreenlockBridge::SetFocusedUser(const AccountId& account_id) {
  if (account_id == focused_account_id_) {
    return;
  }
  focused_account_id_ = account_id;
  for (auto& observer : observers_) {
    observer.OnFocusedUserChanged(account_id);
  }
}

bool ScreenlockBridge::IsLocked() const {
  return lock_handler_ != nullptr;
}

void ScreenlockBridge::Lock() {
  ash::SessionManagerClient::Get()->RequestLockScreen();
}

void ScreenlockBridge::Unlock(const AccountId& account_id) {
  if (lock_handler_) {
    lock_handler_->Unlock(account_id);
  }
}

void ScreenlockBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScreenlockBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ScreenlockBridge::ScreenlockBridge()
    : lock_handler_(nullptr), focused_account_id_(EmptyAccountId()) {}

ScreenlockBridge::~ScreenlockBridge() {}

}  // namespace proximity_auth
