// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/on_device_encryption_data_type_specific_metrics_reporter.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

namespace {

using DataType = OnDeviceEncryptionDataTypeSpecificMetricsReporter::DataType;

std::string_view DataTypeToHistogramSuffix(DataType data_type) {
  switch (data_type) {
    case DataType::kPasskeys:
      return "Passkeys";
    case DataType::kPasswords:
      return "Passwords";
  }
  NOTREACHED();
}

std::string_view GetLastReportingTimePrefName(DataType data_type) {
  switch (data_type) {
    case DataType::kPasskeys:
      return prefs::kOnDeviceEncryptionStatePasskeyLastReportingTime;
    case DataType::kPasswords:
      return prefs::kOnDeviceEncryptionStatePasswordLastReportingTime;
  }
  NOTREACHED();
}

std::string_view GetLastReportedBucketPrefName(DataType data_type) {
  switch (data_type) {
    case DataType::kPasskeys:
      return prefs::kOnDeviceEncryptionStatePasskeyLastReportedBucket;
    case DataType::kPasswords:
      return prefs::kOnDeviceEncryptionStatePasswordLastReportedBucket;
  }
  NOTREACHED();
}

std::optional<OnDeviceEncryptionStateHistogramBucket>
ToOnDeviceEncryptionStateHistogramBucket(OnDeviceEncryptionState state) {
  switch (state) {
    case OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable:
      // While services are initializing or loading, the encryption state cannot
      // be determined yet. We don't want to record metrics for this
      // transitional state.
      return std::nullopt;
    case OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled:
      return OnDeviceEncryptionStateHistogramBucket::
          kOnDeviceEncryptionNotEnabled;
    case OnDeviceEncryptionState::kDeviceNotReady:
      return OnDeviceEncryptionStateHistogramBucket::kDeviceNotReady;
    case OnDeviceEncryptionState::kDeviceReady:
      return OnDeviceEncryptionStateHistogramBucket::kDeviceReady;
    case OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled:
      return OnDeviceEncryptionStateHistogramBucket::
          kPasswordAndPasskeySyncDisabled;
    case OnDeviceEncryptionState::kProfileNotSignedIn:
      return OnDeviceEncryptionStateHistogramBucket::kProfileNotSignedIn;
    case OnDeviceEncryptionState::kProfileSignInPending:
      return OnDeviceEncryptionStateHistogramBucket::kProfileSignInPending;
  }
  NOTREACHED();
}

}  // namespace

OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    OnDeviceEncryptionDataTypeSpecificMetricsReporter(
        DataType data_type,
        std::unique_ptr<OnDeviceEncryptionStateTracker> tracker,
        PrefService& pref_service)
    : data_type_(data_type),
      tracker_(std::move(tracker)),
      pref_service_(pref_service) {
  CHECK(tracker_);
  // During startup, services are actively initializing, and might also start
  // notifying the observers. On the other hand, it is good practice to avoid
  // publishing metrics immediately on startup. Given this, the initialization
  // of observation and publishing of initial metrics is being deferred.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceEncryptionDataTypeSpecificMetricsReporter::
                         StartObservationAndRecordInitialMetrics,
                     weak_ptr_factory_.GetWeakPtr()),
      kInitialStateReportingDelay);
}

OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    ~OnDeviceEncryptionDataTypeSpecificMetricsReporter() = default;

void OnDeviceEncryptionDataTypeSpecificMetricsReporter::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  timer_.Stop();
  observation_.Reset();
  tracker_.reset();
}

void OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    OnDeviceEncryptionStateChanged(OnDeviceEncryptionState /*previous_state*/,
                                   OnDeviceEncryptionState /*new_state*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeRecordMetrics();
}

void OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    OnDeviceEncryptionStateTrackerShuttingDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  timer_.Stop();
  observation_.Reset();
}

void OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    StartObservationAndRecordInitialMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(tracker_);
  observation_.Observe(tracker_.get());
  MaybeRecordMetrics();
}

void OnDeviceEncryptionDataTypeSpecificMetricsReporter::MaybeRecordMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleNextReporting(MaybeRecordReadiness());
}

base::Time
OnDeviceEncryptionDataTypeSpecificMetricsReporter::MaybeRecordReadiness() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(tracker_);
  std::optional<OnDeviceEncryptionStateHistogramBucket> bucket =
      ToOnDeviceEncryptionStateHistogramBucket(tracker_->GetEncryptionState());
  const base::Time now = base::Time::Now();
  base::Time next_scheduled_reporting_time;

  bool current_state_cannot_be_reported = !bucket.has_value();

  if (current_state_cannot_be_reported) {
    next_scheduled_reporting_time = now + kPeriodicReportingDelay;
  } else {
    const auto [last_reported_time, last_reported_bucket] =
        GetLastReportedTimeAndBucket();

    bool metric_has_never_been_reported = last_reported_time.is_null();
    bool metric_has_been_reported_long_ago =
        now - last_reported_time >= kPeriodicReportingDelay;
    bool state_changed_since_last_reporting = bucket != last_reported_bucket;
    // Detecting a potential edge case: when `last_reported_time` is in the
    // future (e.g., due to the system clock change). The safest option in this
    // case is to publish a metric (this might lead to the fact that we publish
    // a redundant event - but, this does not harm).
    bool reporting_time_mismatch_detected = now < last_reported_time;

    bool current_state_must_be_reported =
        metric_has_never_been_reported || state_changed_since_last_reporting ||
        metric_has_been_reported_long_ago || reporting_time_mismatch_detected;

    if (current_state_must_be_reported) {
      base::UmaHistogramEnumeration(
          base::StrCat({"PasswordManager.OnDeviceEncryptionState.",
                        DataTypeToHistogramSuffix(data_type_)}),
          *bucket);
      SetLastReportedTimeAndBucket(now, *bucket);
      next_scheduled_reporting_time = now + kPeriodicReportingDelay;
    } else {
      next_scheduled_reporting_time =
          last_reported_time + kPeriodicReportingDelay;
    }
  }
  return next_scheduled_reporting_time;
}

void OnDeviceEncryptionDataTypeSpecificMetricsReporter::ScheduleNextReporting(
    base::Time execution_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Start(
      FROM_HERE, execution_time,
      base::BindOnce(&OnDeviceEncryptionDataTypeSpecificMetricsReporter::
                         MaybeRecordMetrics,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::pair<base::Time, std::optional<OnDeviceEncryptionStateHistogramBucket>>
OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    GetLastReportedTimeAndBucket() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Time time =
      pref_service_->GetTime(GetLastReportingTimePrefName(data_type_));
  const int bucket =
      pref_service_->GetInteger(GetLastReportedBucketPrefName(data_type_));
  if (time.is_null() || bucket < 0 ||
      bucket >
          static_cast<int>(OnDeviceEncryptionStateHistogramBucket::kMaxValue)) {
    return {base::Time(), std::nullopt};
  }

  return {time, static_cast<OnDeviceEncryptionStateHistogramBucket>(bucket)};
}

void OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    SetLastReportedTimeAndBucket(
        base::Time time,
        OnDeviceEncryptionStateHistogramBucket bucket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetTime(GetLastReportingTimePrefName(data_type_), time);
  pref_service_->SetInteger(GetLastReportedBucketPrefName(data_type_),
                            static_cast<int>(bucket));
}

}  // namespace password_manager
