// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager.h"

#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"

namespace webauthn {
PasskeyUnlockManager::PasskeyUnlockManager(Profile* profile) {}

PasskeyUnlockManager::~PasskeyUnlockManager() = default;

void PasskeyUnlockManager::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void PasskeyUnlockManager::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

bool PasskeyUnlockManager::ShouldDisplayErrorUi() {
  // TODO(crbug.com/450238902): Implement this method: check if passkeys are
  // locked and if they are unlockable.
  return false;
}

void PasskeyUnlockManager::OpenTabForPasskeyUnlockHandler(Browser* browser) {
  // TODO(crbug.com/449986753): Implement.
  NOTIMPLEMENTED();
}

void PasskeyUnlockManager::NotifyObservers() {
  // TODO(crbug.com/450271136): Implement and call when the state changes.
  NOTIMPLEMENTED();
}

void PasskeyUnlockManager::AsynchronouslyCheckGpmPinAvailability() {
  // TODO(crbug.com/449948649): Implement and call in the constructor.
  NOTIMPLEMENTED();
}

void PasskeyUnlockManager::AsynchronouslyCheckSystemUVAvailability() {
  // TODO(crbug.com/450271375): Implement and call in the constructor.
  NOTIMPLEMENTED();
}

void PasskeyUnlockManager::AsynchronouslyLoadEnclaveManager() {
  // TODO(crbug.com/449949272): Implement and call in the constructor.
  NOTIMPLEMENTED();
}

}  // namespace webauthn
