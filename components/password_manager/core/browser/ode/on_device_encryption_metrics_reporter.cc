// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/on_device_encryption_metrics_reporter.h"

#include <memory>
#include <utility>

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_data_type_specific_metrics_reporter.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

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
}

OnDeviceEncryptionMetricsReporter::OnDeviceEncryptionMetricsReporter(
    std::unique_ptr<OnDeviceEncryptionStateTracker> passkey_tracker,
    std::unique_ptr<OnDeviceEncryptionStateTracker> password_tracker,
    PrefService& pref_service)
    : passkey_reporter_(
          kPasskeyOnDeviceEncryptionStateHistogram,
          prefs::kOnDeviceEncryptionStatePasskeyLastReportingTime,
          prefs::kOnDeviceEncryptionStatePasskeyLastReportedBucket,
          std::move(passkey_tracker),
          pref_service),
      password_reporter_(
          kPasswordOnDeviceEncryptionStateHistogram,
          prefs::kOnDeviceEncryptionStatePasswordLastReportingTime,
          prefs::kOnDeviceEncryptionStatePasswordLastReportedBucket,
          std::move(password_tracker),
          pref_service) {}

OnDeviceEncryptionMetricsReporter::~OnDeviceEncryptionMetricsReporter() =
    default;

void OnDeviceEncryptionMetricsReporter::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  passkey_reporter_.Shutdown();
  password_reporter_.Shutdown();
}

}  // namespace password_manager
