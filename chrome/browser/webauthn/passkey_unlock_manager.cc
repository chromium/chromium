// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager.h"

#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "device/fido/features.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/l10n/l10n_util.h"

namespace webauthn {
// TODO(crbug.com/456454164): Don't pass the profile directly to the
// constructor.
PasskeyUnlockManager::PasskeyUnlockManager(Profile* profile) {
  EnclaveManager* enclave_manager = static_cast<EnclaveManager*>(
      EnclaveManagerFactory::GetForProfile(profile));
  enclave_manager_observation_.Observe(enclave_manager);
  passkey_model_observation_.Observe(
      PasskeyModelFactory::GetForProfile(profile));
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (sync_service) {
    sync_service_observation_.Observe(sync_service);
  }
  if (enclave_manager->is_loaded()) {
    enclave_ready_ = enclave_manager->is_ready();
  } else {
    AsynchronouslyLoadEnclaveManager();
  }
  UpdateHasPasskeys();
  UpdateSyncState();
  AsynchronouslyCheckSystemUVAvailability();
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

bool PasskeyUnlockManager::ShouldDisplayErrorUi() const {
  return ArePasskeysLocked() && ArePasskeysUnlockable();
}

void PasskeyUnlockManager::OpenTabWithPasskeyUnlockChallenge(Browser* browser) {
  NavigateParams params(GetSingletonTabNavigateParams(
      browser, GaiaUrls::GetInstance()->signin_chrome_passkey_unlock_url()));
  Navigate(&params);
}

std::u16string PasskeyUnlockManager::GetPasskeyErrorProfilePillTitle(
    ExperimentArm experiment_arm) const {
  switch (experiment_arm) {
    case ExperimentArm::kUnlock:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_UNLOCK);
    case ExperimentArm::kGet:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_GET);
    case ExperimentArm::kVerify:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_VERIFY);
  }
}

std::u16string PasskeyUnlockManager::GetPasskeyErrorProfileMenuDetails(
    ExperimentArm experiment_arm) {
  switch (experiment_arm) {
    case ExperimentArm::kUnlock:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_UNLOCK);
    case ExperimentArm::kGet:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_GET);
    case ExperimentArm::kVerify:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_VERIFY);
  }
}

std::u16string PasskeyUnlockManager::GetPasskeyErrorProfileMenuButtonLabel(
    ExperimentArm experiment_arm) {
  switch (experiment_arm) {
    case ExperimentArm::kUnlock:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_UNLOCK);
    case ExperimentArm::kGet:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_GET);
    case ExperimentArm::kVerify:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_VERIFY);
  }
}

// static
bool PasskeyUnlockManager::IsPasskeyUnlockErrorUiEnabled() {
  return base::FeatureList::IsEnabled(device::kPasskeyUnlockErrorUi) &&
         base::FeatureList::IsEnabled(device::kPasskeyUnlockManager) &&
         base::FeatureList::IsEnabled(device::kWebAuthnOpportunisticRetrieval);
}

void PasskeyUnlockManager::NotifyObserversForTesting() {
  NotifyObservers();
}

PasskeyModel* PasskeyUnlockManager::passkey_model() {
  return passkey_model_observation_.GetSource();
}

EnclaveManager* PasskeyUnlockManager::enclave_manager() {
  return enclave_manager_observation_.GetSource();
}

syncer::SyncService* PasskeyUnlockManager::sync_service() {
  return sync_service_observation_.GetSource();
}

void PasskeyUnlockManager::UpdateHasPasskeys() {
  has_passkeys_ = !passkey_model()->GetAllPasskeys().empty();
}

void PasskeyUnlockManager::UpdateSyncState() {
  sync_active_ =
      sync_service() &&
      sync_service()->GetActiveDataTypes().Has(syncer::WEBAUTHN_CREDENTIAL) &&
      sync_service()->GetUserActionableError() ==
          syncer::SyncService::UserActionableError::kNone;
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
  enclave_manager()->AreUserVerifyingKeysSupported(
      base::BindOnce(&PasskeyUnlockManager::OnHaveSystemUVAvailability,
                     weak_ptr_factory_.GetWeakPtr()));
}
void PasskeyUnlockManager::OnHaveSystemUVAvailability(bool has_system_uv) {
  bool error_ui_was_visible = ShouldDisplayErrorUi();
  has_system_uv_ = has_system_uv;
  if (error_ui_was_visible != ShouldDisplayErrorUi()) {
    NotifyObservers();
  }
}

void PasskeyUnlockManager::AsynchronouslyLoadEnclaveManager() {
  auto callback = base::BindOnce(&PasskeyUnlockManager::OnStateUpdated,
                                 weak_ptr_factory_.GetWeakPtr());
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  auto delayed_task =
      base::BindOnce(&EnclaveManager::Load, enclave_manager()->GetWeakPtr(),
                     std::move(callback));
  task_runner->PostDelayedTask(FROM_HERE, std::move(delayed_task),
                               base::Minutes(4));
}

bool PasskeyUnlockManager::ArePasskeysLocked() const {
  // TODO(crbug.com/450238902): Also check enclave manager readiness.
  return has_passkeys_.value_or(false) && sync_active_;
}

bool PasskeyUnlockManager::ArePasskeysUnlockable() const {
  // TODO(crbug.com/450238902): Also check GPM PIN availability.
  // TODO(crbug.com/450551870): Check for more verification methods.
  return has_system_uv_.value_or(false);
}

void PasskeyUnlockManager::Shutdown() {
  for (Observer& observer : observer_list_) {
    observer.OnPasskeyUnlockManagerShuttingDown();
  }
  enclave_manager_observation_.Reset();
  passkey_model_observation_.Reset();
  sync_service_observation_.Reset();
}

void PasskeyUnlockManager::OnStateUpdated() {
  enclave_ready_ = enclave_manager()->is_ready();
  NotifyObservers();
}

void PasskeyUnlockManager::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  UpdateHasPasskeys();
  NotifyObservers();
}

void PasskeyUnlockManager::OnPasskeyModelShuttingDown() {}

void PasskeyUnlockManager::OnPasskeyModelIsReady(bool is_ready) {
  UpdateHasPasskeys();
  NotifyObservers();
}

void PasskeyUnlockManager::OnStateChanged(syncer::SyncService* sync) {
  bool error_ui_was_visible = ShouldDisplayErrorUi();
  UpdateSyncState();
  if (error_ui_was_visible != ShouldDisplayErrorUi()) {
    // Only notify observers if the sync state changed.
    NotifyObservers();
  }
}

void PasskeyUnlockManager::OnSyncShutdown(syncer::SyncService* sync) {
  NOTREACHED();
}

PasskeyUnlockManager::PasskeyUnlockManager() = default;

}  // namespace webauthn
