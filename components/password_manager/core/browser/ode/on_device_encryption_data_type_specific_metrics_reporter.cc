// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/on_device_encryption_data_type_specific_metrics_reporter.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

namespace {

// Upper bound for the time-to-unlock histogram (in seconds) before overflowing
// into the max bucket.
constexpr base::TimeDelta kMaxTimeToUnlockDuration = base::Days(30);
constexpr size_t kTimeToUnlockBucketCount = 100;

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

std::string_view GetFirstUnlockTimePrefName(DataType data_type) {
  switch (data_type) {
    case DataType::kPasskeys:
      return prefs::kOnDeviceEncryptionPasskeyFirstUnlockTime;
    case DataType::kPasswords:
      return prefs::kOnDeviceEncryptionPasswordFirstUnlockTime;
  }
  NOTREACHED();
}

std::string_view GetTimeToUnlockLastReportingTimePrefName(DataType data_type) {
  switch (data_type) {
    case DataType::kPasskeys:
      return prefs::kOnDeviceEncryptionPasskeyTimeToUnlockLastReportingTime;
    case DataType::kPasswords:
      return prefs::kOnDeviceEncryptionPasswordTimeToUnlockLastReportingTime;
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

bool ShouldRecordPeriodicReport(base::Time last_reported_time, base::Time now) {
  // Also trigger reporting if `last_reported_time` is in the future (e.g., due
  // to a system clock change) to heal the stored timestamp.
  return last_reported_time.is_null() ||
         now - last_reported_time >= kPeriodicReportingDelay ||
         now < last_reported_time;
}

void RecordTimeToUnlockHistogram(DataType data_type,
                                 base::TimeDelta time_to_unlock) {
  base::UmaHistogramCustomCounts(
      base::StrCat({"PasswordManager.OnDeviceEncryptionTimeToUnlock.",
                    DataTypeToHistogramSuffix(data_type)}),
      base::saturated_cast<int>(time_to_unlock.InSeconds()),
      /*min=*/1,
      /*exclusive_max=*/
      base::checked_cast<int>(kMaxTimeToUnlockDuration.InSeconds()),
      /*buckets=*/kTimeToUnlockBucketCount);
}

}  // namespace

OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    OnDeviceEncryptionDataTypeSpecificMetricsReporter(
        DataType data_type,
        std::unique_ptr<OnDeviceEncryptionStateTracker> tracker,
        PrefService* pref_service)
    : data_type_(data_type),
      tracker_(std::move(tracker)),
      pref_service_(CHECK_DEREF(pref_service)) {
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
    ClearCachedTimeToUnlockData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(GetFirstUnlockTimePrefName(data_type_));
  pref_service_->ClearPref(
      GetTimeToUnlockLastReportingTimePrefName(data_type_));
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
  const base::Time now = base::Time::Now();
  const base::Time next_readiness_time =
      MaybeRecordReadinessAndSuggestNextReportingTime(now);
  const base::Time next_time_to_unlock_time =
      MaybeRecordTimeToUnlockAndSuggestNextReportingTime(now);
  ScheduleNextReporting(
      std::min(next_readiness_time, next_time_to_unlock_time));
}

base::Time OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    MaybeRecordReadinessAndSuggestNextReportingTime(base::Time now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(tracker_);
  std::optional<OnDeviceEncryptionStateHistogramBucket> bucket =
      ToOnDeviceEncryptionStateHistogramBucket(tracker_->GetEncryptionState());
  base::Time next_scheduled_reporting_time;

  bool current_state_cannot_be_reported = !bucket.has_value();

  if (current_state_cannot_be_reported) {
    next_scheduled_reporting_time = now + kPeriodicReportingDelay;
  } else {
    const auto [last_reported_time, last_reported_bucket] =
        GetLastReportedTimeAndBucket();

    const bool current_state_must_be_reported =
        bucket != last_reported_bucket ||
        ShouldRecordPeriodicReport(last_reported_time, now);

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

base::Time OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    MaybeRecordTimeToUnlockAndSuggestNextReportingTime(base::Time now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(tracker_);

  if (tracker_->GetEncryptionState() != OnDeviceEncryptionState::kDeviceReady) {
    return now + kPeriodicReportingDelay;
  }

  const base::Time sign_in_time =
      pref_service_->GetTime(prefs::kOnDeviceEncryptionLastSignInTime);
  if (sign_in_time.is_null() || now < sign_in_time) {
    // Do not persist or emit durations if sign-in time is missing or in the
    // future (e.g., due to a backward system clock jump).
    return now + kPeriodicReportingDelay;
  }

  // Check feature enablement after eligibility checks so any associated field
  // trial is activated only for eligible profiles.
  if (!base::FeatureList::IsEnabled(
          features::kPasswordManagerOnDeviceEncryptionTimeToUnlockMetrics)) {
    return now + kPeriodicReportingDelay;
  }

  const std::string_view first_unlock_pref =
      GetFirstUnlockTimePrefName(data_type_);
  base::Time first_unlock_time = pref_service_->GetTime(first_unlock_pref);
  if (first_unlock_time.is_null() || first_unlock_time < sign_in_time) {
    first_unlock_time = now;
    pref_service_->SetTime(first_unlock_pref, first_unlock_time);
  }

  const base::TimeDelta time_to_unlock = first_unlock_time - sign_in_time;

  const std::string_view last_reporting_time_pref =
      GetTimeToUnlockLastReportingTimePrefName(data_type_);
  const base::Time last_reported_time =
      pref_service_->GetTime(last_reporting_time_pref);
  if (!ShouldRecordPeriodicReport(last_reported_time, now)) {
    return last_reported_time + kPeriodicReportingDelay;
  }

  RecordTimeToUnlockHistogram(data_type_, time_to_unlock);
  pref_service_->SetTime(last_reporting_time_pref, now);
  return now + kPeriodicReportingDelay;
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
