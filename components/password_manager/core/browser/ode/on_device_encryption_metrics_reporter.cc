// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/on_device_encryption_metrics_reporter.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_data_type_specific_metrics_reporter.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"

namespace password_manager {

// static
void OnDeviceEncryptionMetricsReporter::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(
      prefs::kOnDeviceEncryptionStatePasskeyLastReportingTime, base::Time());
  registry->RegisterIntegerPref(
      prefs::kOnDeviceEncryptionStatePasskeyLastReportedBucket,
      kNoReportedBucket);
  registry->RegisterTimePref(
      prefs::kOnDeviceEncryptionStatePasswordLastReportingTime, base::Time());
  registry->RegisterIntegerPref(
      prefs::kOnDeviceEncryptionStatePasswordLastReportedBucket,
      kNoReportedBucket);

  registry->RegisterTimePref(prefs::kOnDeviceEncryptionLastSignInTime,
                             base::Time());
  registry->RegisterTimePref(prefs::kOnDeviceEncryptionPasskeyFirstUnlockTime,
                             base::Time());
  registry->RegisterTimePref(
      prefs::kOnDeviceEncryptionPasskeyTimeToUnlockLastReportingTime,
      base::Time());
  registry->RegisterTimePref(prefs::kOnDeviceEncryptionPasswordFirstUnlockTime,
                             base::Time());
  registry->RegisterTimePref(
      prefs::kOnDeviceEncryptionPasswordTimeToUnlockLastReportingTime,
      base::Time());
}

OnDeviceEncryptionMetricsReporter::OnDeviceEncryptionMetricsReporter(
    std::unique_ptr<OnDeviceEncryptionStateTracker> passkey_tracker,
    std::unique_ptr<OnDeviceEncryptionStateTracker> password_tracker,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager)
    : pref_service_(CHECK_DEREF(pref_service)),
      passkey_reporter_(OnDeviceEncryptionDataTypeSpecificMetricsReporter::
                            DataType::kPasskeys,
                        std::move(passkey_tracker),
                        pref_service),
      password_reporter_(OnDeviceEncryptionDataTypeSpecificMetricsReporter::
                             DataType::kPasswords,
                         std::move(password_tracker),
                         pref_service) {
  // Only track time-to-unlock state and observe `IdentityManager` when the
  // killswitch allows it.
  if (identity_manager &&
      base::FeatureList::IsEnabled(
          features::kPasswordManagerOnDeviceEncryptionTimeToUnlockMetrics)) {
    // Clear any leftover time-to-unlock prefs on startup if the profile is
    // signed out, e.g. if a sign-out occurred before this reporter started
    // observing.
    if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      ClearAllTimeToUnlockData();
    }
    identity_manager_observation_.Observe(identity_manager);
  }
}

OnDeviceEncryptionMetricsReporter::~OnDeviceEncryptionMetricsReporter() =
    default;

void OnDeviceEncryptionMetricsReporter::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  identity_manager_observation_.Reset();
  passkey_reporter_.Shutdown();
  password_reporter_.Shutdown();
}

void OnDeviceEncryptionMetricsReporter::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      // Ignore non-`kSignin` updates (e.g., sync consent changes for the same
      // account) so the current sign-in session's timestamps remain intact.
      return;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      ClearAllTimeToUnlockData();
      return;
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      // Reset cached unlock/reporting state and record the new sign-in
      // timestamp.
      passkey_reporter_.ClearCachedTimeToUnlockData();
      password_reporter_.ClearCachedTimeToUnlockData();
      pref_service_->SetTime(prefs::kOnDeviceEncryptionLastSignInTime,
                             base::Time::Now());
      // On sign-in or account switch, Sync resets and initializes
      // asynchronously, so trackers only become ready after this timestamp is
      // saved. Sync then triggers state-change updates, allowing the
      // reporters to record readiness and time-to-unlock metrics.
      return;
  }
}

void OnDeviceEncryptionMetricsReporter::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  identity_manager_observation_.Reset();
}

void OnDeviceEncryptionMetricsReporter::ClearAllTimeToUnlockData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(prefs::kOnDeviceEncryptionLastSignInTime);
  passkey_reporter_.ClearCachedTimeToUnlockData();
  password_reporter_.ClearCachedTimeToUnlockData();
}

}  // namespace password_manager
