// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager.h"

#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "google_apis/gaia/gaia_urls.h"

namespace webauthn {
PasskeyUnlockManager::PasskeyUnlockManager(Profile* profile) {
  passkey_model_observation_.Observe(
      PasskeyModelFactory::GetForProfile(profile));
  UpdateHasPasskeys();
  NotifyObservers();
}

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

void PasskeyUnlockManager::OpenTabWithPasskeyUnlockChallenge(Browser* browser) {
  NavigateParams params(GetSingletonTabNavigateParams(
      browser, GaiaUrls::GetInstance()->signin_chrome_passkey_unlock_url()));
  Navigate(&params);
}

PasskeyModel* PasskeyUnlockManager::passkey_model() {
  return passkey_model_observation_.GetSource();
}

void PasskeyUnlockManager::UpdateHasPasskeys() {
  has_passkeys_ = !passkey_model()->GetAllPasskeys().empty();
}

void PasskeyUnlockManager::NotifyObservers() {
  for (Observer& observer : observer_list_) {
    observer.OnPasskeyUnlockManagerStateChanged();
  }
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

// webauthn::PasskeyModel::Observer
void PasskeyUnlockManager::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  UpdateHasPasskeys();
  NotifyObservers();
}

// webauthn::PasskeyModel::Observer
void PasskeyUnlockManager::OnPasskeyModelShuttingDown() {}

// webauthn::PasskeyModel::Observer
void PasskeyUnlockManager::OnPasskeyModelIsReady(bool is_ready) {
  UpdateHasPasskeys();
  NotifyObservers();
}

}  // namespace webauthn
