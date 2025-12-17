// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager.h"

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "device/fido/features.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/l10n/l10n_util.h"

namespace webauthn {

static constexpr const char kPasskeyReadinessHistogram[] =
    "WebAuthentication.PasskeyReadiness";

// TODO(crbug.com/456454164): Don't pass the profile directly to the
// constructor.
PasskeyUnlockManager::PasskeyUnlockManager(Profile* profile) {
  EnclaveManagerInterface* enclave_manager =
      EnclaveManagerFactory::GetForProfile(profile);
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
    MaybeRecordDelayedPasskeyReadinessHistogram();
  } else {
    enclave_manager->LoadAfterDelay(
        base::Minutes(4), base::BindOnce(&PasskeyUnlockManager::OnStateUpdated,
                                         weak_ptr_factory_.GetWeakPtr()));
  }
  UpdateHasPasskeys();
  MaybeRecordDelayedPasskeyCountHistogram();
  UpdateSyncState();
  AsynchronouslyCheckSystemUVAvailability();
  AsynchronouslyCheckGpmPinAvailability();
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
  return should_display_error_ui_;
}

void PasskeyUnlockManager::ComputeShouldDisplayErrorUiAndNotifyObservers() {
  bool previous_value = should_display_error_ui_;
  // Compute the new value of `should_display_error_ui_`.
  std::optional<bool> passkeys_are_locked = ArePasskeysLocked();
  std::optional<bool> passkeys_are_unlockable = ArePasskeysUnlockable();
  if (!passkeys_are_locked.has_value() ||
      !passkeys_are_unlockable.has_value()) {
    // If we don't have sufficient information to decide whether passkeys are
    // locked and unlockable, we don't want to display the error UI.
    should_display_error_ui_ = false;
  } else {
    should_display_error_ui_ =
        passkeys_are_locked.value() && passkeys_are_unlockable.value();
  }
  // If `should_display_error_ui_` changed - notify observers.
  if (should_display_error_ui_ != previous_value) {
    NotifyObservers();
  }
}

void PasskeyUnlockManager::NotifyObservers() {
  for (Observer& observer : observer_list_) {
    observer.OnPasskeyUnlockManagerStateChanged();
  }
}

void PasskeyUnlockManager::OpenTabWithPasskeyUnlockChallenge(Browser* browser) {
  NavigateParams params(GetSingletonTabNavigateParams(
      browser, GaiaUrls::GetInstance()->signin_chrome_passkey_unlock_url()));
  // Allow the window to close itself.
  params.opened_by_another_window = true;
  Navigate(&params);
}

std::u16string PasskeyUnlockManager::GetPasskeyErrorProfilePillTitle() const {
  switch (device::kPasskeyUnlockErrorUiExperimentArm.Get()) {
    case device::PasskeyUnlockErrorUiExperimentArm::kUnlock:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_UNLOCK);
    case device::PasskeyUnlockErrorUiExperimentArm::kGet:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_GET);
    case device::PasskeyUnlockErrorUiExperimentArm::kVerify:
      return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_VERIFY);
  }
}

std::u16string PasskeyUnlockManager::GetPasskeyErrorProfileMenuDetails() const {
  switch (device::kPasskeyUnlockErrorUiExperimentArm.Get()) {
    case device::PasskeyUnlockErrorUiExperimentArm::kUnlock:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_UNLOCK);
    case device::PasskeyUnlockErrorUiExperimentArm::kGet:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_GET);
    case device::PasskeyUnlockErrorUiExperimentArm::kVerify:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_VERIFY);
  }
}

std::u16string PasskeyUnlockManager::GetPasskeyErrorProfileMenuButtonLabel()
    const {
  switch (device::kPasskeyUnlockErrorUiExperimentArm.Get()) {
    case device::PasskeyUnlockErrorUiExperimentArm::kUnlock:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_UNLOCK);
    case device::PasskeyUnlockErrorUiExperimentArm::kGet:
      return l10n_util::GetStringUTF16(
          IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_GET);
    case device::PasskeyUnlockErrorUiExperimentArm::kVerify:
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

void PasskeyUnlockManager::RecordErrorUIEventType(ErrorUIEventType event_type) {
  base::UmaHistogramEnumeration("WebAuthentication.PasskeyUnlock.ErrorUi.Event",
                                event_type);
}

PasskeyModel* PasskeyUnlockManager::passkey_model() {
  return passkey_model_observation_.GetSource();
}

EnclaveManagerInterface* PasskeyUnlockManager::enclave_manager() {
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

void PasskeyUnlockManager::AsynchronouslyCheckGpmPinAvailability() {
  enclave_manager()->CheckGpmPinAvailability(
      base::BindOnce(&PasskeyUnlockManager::OnHaveGpmPinAvailability,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasskeyUnlockManager::OnHaveGpmPinAvailability(
    EnclaveManager::GpmPinAvailability gpm_pin_availability) {
  has_gpm_pin_ = gpm_pin_availability ==
                 EnclaveManager::GpmPinAvailability::kGpmPinSetAndUsable;
  ComputeShouldDisplayErrorUiAndNotifyObservers();
  MaybeRecordDelayedGpmPinStatusHistogram(gpm_pin_availability);
}

void PasskeyUnlockManager::AsynchronouslyCheckSystemUVAvailability() {
  EnclaveManager::AreUserVerifyingKeysSupported(
      base::BindOnce(&PasskeyUnlockManager::OnHaveSystemUVAvailability,
                     weak_ptr_factory_.GetWeakPtr()));
}
void PasskeyUnlockManager::OnHaveSystemUVAvailability(bool has_system_uv) {
  has_system_uv_ = has_system_uv;
  ComputeShouldDisplayErrorUiAndNotifyObservers();
}

std::optional<bool> PasskeyUnlockManager::ArePasskeysLocked() const {
  if (!has_passkeys_.has_value()) {
    return std::nullopt;
  }
  if (!enclave_ready_.has_value()) {
    return std::nullopt;
  }
  return has_passkeys_.value() && sync_active_ && !enclave_ready_.value();
}

std::optional<bool> PasskeyUnlockManager::ArePasskeysUnlockable() const {
  if (!has_system_uv_.has_value() && !has_gpm_pin_.has_value()) {
    return std::nullopt;
  }
  // TODO(crbug.com/450551870): Check for more verification methods.
  return has_system_uv_.value_or(false) || has_gpm_pin_.value_or(false);
}

void PasskeyUnlockManager::Shutdown() {
  for (Observer& observer : observer_list_) {
    observer.OnPasskeyUnlockManagerShuttingDown();
  }
  enclave_manager_observation_.Reset();
  passkey_model_observation_.Reset();
  sync_service_observation_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void PasskeyUnlockManager::OnStateUpdated() {
  enclave_ready_ = enclave_manager()->is_ready();
  ComputeShouldDisplayErrorUiAndNotifyObservers();
  MaybeRecordDelayedPasskeyReadinessHistogram();
}

void PasskeyUnlockManager::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  UpdateHasPasskeys();
  ComputeShouldDisplayErrorUiAndNotifyObservers();
}

void PasskeyUnlockManager::OnPasskeyModelShuttingDown() {}

void PasskeyUnlockManager::OnPasskeyModelIsReady(bool is_ready) {
  UpdateHasPasskeys();
  ComputeShouldDisplayErrorUiAndNotifyObservers();
  // If the passkey model wasn't ready on startup, record the histogram now.
  MaybeRecordDelayedPasskeyCountHistogram();
}

void PasskeyUnlockManager::OnStateChanged(syncer::SyncService* sync) {
  UpdateSyncState();
  ComputeShouldDisplayErrorUiAndNotifyObservers();
}

void PasskeyUnlockManager::OnSyncShutdown(syncer::SyncService* sync) {
  NOTREACHED();
}

PasskeyUnlockManager::PasskeyUnlockManager() = default;

void PasskeyUnlockManager::MaybeRecordDelayedPasskeyCountHistogram() {
  if (passkey_count_recorded_on_startup_ || !passkey_model()->IsReady()) {
    return;
  }
  passkey_count_recorded_on_startup_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PasskeyUnlockManager::RecordPasskeyCountHistogram,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(30));
}

void PasskeyUnlockManager::RecordPasskeyCountHistogram() {
  base::UmaHistogramCounts1000("WebAuthentication.PasskeyCount",
                               passkey_model()->GetAllPasskeys().size());
}

void PasskeyUnlockManager::MaybeRecordDelayedPasskeyReadinessHistogram() {
  if (passkey_readiness_recorded_on_startup_ ||
      !enclave_manager()->is_loaded()) {
    return;
  }
  passkey_readiness_recorded_on_startup_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PasskeyUnlockManager::RecordPasskeyReadinessHistogram,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(30));
}

void PasskeyUnlockManager::RecordPasskeyReadinessHistogram() {
  base::UmaHistogramBoolean(kPasskeyReadinessHistogram,
                            enclave_manager()->is_ready());
}

void PasskeyUnlockManager::MaybeRecordDelayedGpmPinStatusHistogram(
    EnclaveManager::GpmPinAvailability gpm_pin_availability) {
  if (gpm_pin_status_recorded_on_startup_) {
    return;
  }
  gpm_pin_status_recorded_on_startup_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PasskeyUnlockManager::RecordGpmPinStatusHistogram,
                     weak_ptr_factory_.GetWeakPtr(), gpm_pin_availability),
      base::Seconds(30));
}

void PasskeyUnlockManager::RecordGpmPinStatusHistogram(
    EnclaveManager::GpmPinAvailability gpm_pin_availability) {
  base::UmaHistogramEnumeration("WebAuthentication.GpmPinStatus",
                                gpm_pin_availability);
}

}  // namespace webauthn
