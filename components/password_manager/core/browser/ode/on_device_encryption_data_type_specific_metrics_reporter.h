// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_DATA_TYPE_SPECIFIC_METRICS_REPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_DATA_TYPE_SPECIFIC_METRICS_REPORTER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"

class PrefService;

namespace password_manager {

inline constexpr base::TimeDelta kInitialStateReportingDelay =
    base::Seconds(30);

inline constexpr base::TimeDelta kPeriodicReportingDelay = base::Days(1);

// Indicates that no histogram bucket has been reported yet.
inline constexpr int kNoReportedBucket = -1;

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
// passkeys) and publishes the corresponding readiness metrics periodically.
//
// Metrics are recorded at least once per `kPeriodicReportingDelay`, provided
// that the on-device encryption state is different from
// `OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable`.
// In addition, metrics are recorded immediately whenever the state changes.
//
// Note that recording of the initial state on startup is delayed by
// `kInitialStateReportingDelay`; if the browser terminates sooner than
// `kInitialStateReportingDelay`, the metrics will not be recorded.
class OnDeviceEncryptionDataTypeSpecificMetricsReporter
    : public OnDeviceEncryptionStateTracker::Observer {
 public:
  OnDeviceEncryptionDataTypeSpecificMetricsReporter(
      std::string_view histogram_name,
      std::string_view pref_name_last_reporting_time,
      std::string_view pref_name_last_reported_bucket,
      std::unique_ptr<OnDeviceEncryptionStateTracker> tracker,
      PrefService& pref_service);

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

  // Evaluates whether the current on-device encryption state should be recorded
  // to UMA. A metric is recorded if it has never been reported before, if the
  // state has changed since the last report, or if at least
  // `kPeriodicReportingDelay` has elapsed since the last report. If recorded,
  // updates the last reporting time and bucket in preferences. In all cases,
  // schedules the next evaluation via `ScheduleNextReporting()`.
  void MaybeRecordReadiness();

  // Schedules `timer_` to invoke `MaybeRecordReadiness()` at `execution_time`.
  void ScheduleNextReporting(base::Time execution_time);

  // Reads the last reported timestamp and histogram bucket from
  // `pref_service_`. Returns `{time, std::nullopt}` if no metric has been
  // reported yet.
  std::pair<base::Time, std::optional<OnDeviceEncryptionStateHistogramBucket>>
  GetLastReportedTimeAndBucket() const;

  // Persists the given reporting `time` and histogram `bucket` to
  // `pref_service_`.
  void SetLastReportedTimeAndBucket(
      base::Time time,
      OnDeviceEncryptionStateHistogramBucket bucket);

  const std::string histogram_name_;
  const std::string pref_name_last_reporting_time_;
  const std::string pref_name_last_reported_bucket_;
  std::unique_ptr<OnDeviceEncryptionStateTracker> tracker_;
  const raw_ref<PrefService> pref_service_;

  base::ScopedObservation<OnDeviceEncryptionStateTracker,
                          OnDeviceEncryptionStateTracker::Observer>
      observation_{this};
  base::WallClockTimer timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OnDeviceEncryptionDataTypeSpecificMetricsReporter>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_DATA_TYPE_SPECIFIC_METRICS_REPORTER_H_
