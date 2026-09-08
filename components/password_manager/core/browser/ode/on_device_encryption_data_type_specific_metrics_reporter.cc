// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/on_device_encryption_data_type_specific_metrics_reporter.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"

namespace password_manager {

namespace {

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
        std::string_view histogram_name,
        std::unique_ptr<OnDeviceEncryptionStateTracker> tracker)
    : histogram_name_(histogram_name), tracker_(std::move(tracker)) {
  CHECK(!histogram_name_.empty());
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
  observation_.Reset();
  tracker_.reset();
}

void OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    OnDeviceEncryptionStateChanged(OnDeviceEncryptionState /*previous_state*/,
                                   OnDeviceEncryptionState new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeRecordReadiness(new_state);
}

void OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    OnDeviceEncryptionStateTrackerShuttingDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observation_.Reset();
}

void OnDeviceEncryptionDataTypeSpecificMetricsReporter::
    StartObservationAndRecordInitialMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(tracker_);
  observation_.Observe(tracker_.get());
  MaybeRecordReadiness(tracker_->GetEncryptionState());
}

void OnDeviceEncryptionDataTypeSpecificMetricsReporter::MaybeRecordReadiness(
    OnDeviceEncryptionState current_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<OnDeviceEncryptionStateHistogramBucket> bucket =
      ToOnDeviceEncryptionStateHistogramBucket(current_state);
  if (bucket.has_value() && bucket != last_published_bucket_) {
    last_published_bucket_ = bucket;
    base::UmaHistogramEnumeration(histogram_name_, *bucket);
  }
}

}  // namespace password_manager
