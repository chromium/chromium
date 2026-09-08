// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_DATA_TYPE_SPECIFIC_METRICS_REPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_DATA_TYPE_SPECIFIC_METRICS_REPORTER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"

namespace password_manager {

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

// Tracks the on-device encryption state for a specific data type (passwords or
// passkeys) and publishes the corresponding readiness metrics.
class OnDeviceEncryptionDataTypeSpecificMetricsReporter
    : public OnDeviceEncryptionStateTracker::Observer {
 public:
  OnDeviceEncryptionDataTypeSpecificMetricsReporter(
      std::string_view histogram_name,
      std::unique_ptr<OnDeviceEncryptionStateTracker> tracker);

  OnDeviceEncryptionDataTypeSpecificMetricsReporter(
      const OnDeviceEncryptionDataTypeSpecificMetricsReporter&) = delete;
  OnDeviceEncryptionDataTypeSpecificMetricsReporter& operator=(
      const OnDeviceEncryptionDataTypeSpecificMetricsReporter&) = delete;

  ~OnDeviceEncryptionDataTypeSpecificMetricsReporter() override;

  void Shutdown();

  // OnDeviceEncryptionStateTracker::Observer:
  void OnDeviceEncryptionStateChanged(
      OnDeviceEncryptionState previous_state,
      OnDeviceEncryptionState new_state) override;
  void OnDeviceEncryptionStateTrackerShuttingDown() override;

 private:
  // Starts observing the tracker and records the initial readiness metric.
  void StartObservationAndRecordInitialMetrics();

  // Determines whether the encryption state should be published to metrics (and
  // if yes, publishes the metric).
  void MaybeRecordReadiness(OnDeviceEncryptionState current_state);

  const std::string histogram_name_;
  std::unique_ptr<OnDeviceEncryptionStateTracker> tracker_;

  base::ScopedObservation<OnDeviceEncryptionStateTracker,
                          OnDeviceEncryptionStateTracker::Observer>
      observation_{this};

  std::optional<OnDeviceEncryptionStateHistogramBucket> last_published_bucket_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OnDeviceEncryptionDataTypeSpecificMetricsReporter>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_DATA_TYPE_SPECIFIC_METRICS_REPORTER_H_
