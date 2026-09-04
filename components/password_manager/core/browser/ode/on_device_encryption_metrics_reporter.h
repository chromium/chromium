// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"

namespace password_manager {

inline constexpr char kPasskeyOnDeviceEncryptionStateHistogram[] =
    "PasswordManager.OnDeviceEncryptionState.Passkeys";
inline constexpr char kPasswordOnDeviceEncryptionStateHistogram[] =
    "PasswordManager.OnDeviceEncryptionState.Passwords";

inline constexpr base::TimeDelta kInitialStateReportingDelay =
    base::Seconds(30);

// LINT.IfChange(OnDeviceEncryptionStateHistogramBucket)
enum class OnDeviceEncryptionStateHistogramBucket {
  kOnDeviceEncryptionNotEnabled = 0,
  kDeviceNotReady = 1,
  kDeviceReady = 2,
  kPasswordAndPasskeySyncDisabled = 3,
  kProfileNotSignedIn = 4,
  kProfileSignInPending = 5,
  kMaxValue = kProfileSignInPending,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:OnDeviceEncryptionStateHistogramBucket)

// Tracks the on-device encryption states of passwords and passkeys and
// publishes the corresponding readiness metrics.
class OnDeviceEncryptionMetricsReporter
    : public KeyedService,
      public OnDeviceEncryptionStateTracker::Observer {
 public:
  OnDeviceEncryptionMetricsReporter(
      std::unique_ptr<OnDeviceEncryptionStateTracker> passkey_tracker,
      std::unique_ptr<OnDeviceEncryptionStateTracker> password_tracker);

  OnDeviceEncryptionMetricsReporter(const OnDeviceEncryptionMetricsReporter&) =
      delete;
  OnDeviceEncryptionMetricsReporter& operator=(
      const OnDeviceEncryptionMetricsReporter&) = delete;

  ~OnDeviceEncryptionMetricsReporter() override;

  // KeyedService:
  void Shutdown() override;

  // OnDeviceEncryptionStateTracker::Observer:
  void OnDeviceEncryptionStateChanged(
      OnDeviceEncryptionStateTracker* tracker,
      OnDeviceEncryptionState previous_state,
      OnDeviceEncryptionState new_state) override;
  void OnDeviceEncryptionStateTrackerShuttingDown(
      OnDeviceEncryptionStateTracker* tracker) override;

 private:
  // Starts observing the trackers and recording the readiness metrics.
  void StartObservationsAndRecordInitialMetrics();

  // Determines whether the passkey encryption state should be published to
  // metrics (and if yes, publishes the metric).
  void MaybeRecordPasskeyReadiness(OnDeviceEncryptionState current_state);

  // Determines whether the password encryption state should be published to
  // metrics (and if yes, publishes the metric).
  void MaybeRecordPasswordReadiness(OnDeviceEncryptionState current_state);

  // Returns the corresponding histogram bucket, or std::nullopt if the
  // on-device encryption state does not map to any of the histogram buckets.
  std::optional<OnDeviceEncryptionStateHistogramBucket>
  ToOnDeviceEncryptionStateHistogramBucket(OnDeviceEncryptionState state);

  std::unique_ptr<OnDeviceEncryptionStateTracker> passkey_tracker_;
  std::unique_ptr<OnDeviceEncryptionStateTracker> password_tracker_;

  base::ScopedObservation<OnDeviceEncryptionStateTracker,
                          OnDeviceEncryptionStateTracker::Observer>
      passkey_observation_{this};
  base::ScopedObservation<OnDeviceEncryptionStateTracker,
                          OnDeviceEncryptionStateTracker::Observer>
      password_observation_{this};

  std::optional<OnDeviceEncryptionStateHistogramBucket>
      last_published_passkey_bucket_;
  std::optional<OnDeviceEncryptionStateHistogramBucket>
      last_published_password_bucket_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OnDeviceEncryptionMetricsReporter> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_H_
