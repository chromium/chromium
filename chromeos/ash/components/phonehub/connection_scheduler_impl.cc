// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/connection_scheduler_impl.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/feature_status.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"

namespace ash {
namespace phonehub {

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,               // Number of initial errors to ignore.
    10 * 1000,       // Initial delay of 10 seconds in ms.
    2.0,             // Factor by which the waiting time will be multiplied.
    0.2,             // Fuzzing percentage.
    60 * 60 * 1000,  // Maximum delay of 1 hour in ms.
    -1,              // Never discard the entry.
    true,            // Use initial delay.
};

ConnectionSchedulerImpl::ConnectionSchedulerImpl(
    secure_channel::ConnectionManager* connection_manager,
    FeatureStatusProvider* feature_status_provider)
    : connection_manager_(connection_manager),
      feature_status_provider_(feature_status_provider),
      retry_backoff_(&kRetryBackoffPolicy) {
  DCHECK(connection_manager_);
  DCHECK(feature_status_provider_);

  current_feature_status_ = feature_status_provider_->GetStatus();
  feature_status_provider_->AddObserver(this);
}

ConnectionSchedulerImpl::~ConnectionSchedulerImpl() {
  feature_status_provider_->RemoveObserver(this);
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ConnectionSchedulerImpl::ScheduleConnectionNow() {
  if (feature_status_provider_->GetStatus() !=
      FeatureStatus::kEnabledButDisconnected) {
    PA_LOG(WARNING) << "ScheduleConnectionNow() could not request a connection "
                    << "attempt because the current status is: "
                    << feature_status_provider_->GetStatus() << ".";
    return;
  }

  connection_manager_->AttemptNearbyConnection();
}

void ConnectionSchedulerImpl::OnFeatureStatusChanged() {
  const FeatureStatus previous_feature_status = current_feature_status_;
  current_feature_status_ = feature_status_provider_->GetStatus();

  switch (current_feature_status_) {
    // The following feature states indicate that there is an interruption with
    // establishing connection to the host phone or that the feature is blocked
    // from initiating a connection. Disconnect the existing connection, reset
    // backoffs, and return early.
    case FeatureStatus::kNotEligibleForFeature:
      [[fallthrough]];
    case FeatureStatus::kEligiblePhoneButNotSetUp:
      [[fallthrough]];
    case FeatureStatus::kPhoneSelectedAndPendingSetup:
      [[fallthrough]];
    case FeatureStatus::kDisabled:
      [[fallthrough]];
    case FeatureStatus::kUnavailableBluetoothOff:
      [[fallthrough]];
    case FeatureStatus::kLockOrSuspended:
      DisconnectAndClearBackoffAttempts();
      return;

    // Connection has been established, clear existing backoffs and return
    // early.
    case FeatureStatus::kEnabledAndConnected:
      ClearBackoffAttempts();
      return;

    // Connection is in progress, return and wait for the result.
    case FeatureStatus::kEnabledAndConnecting:
      return;

    // Phone is available for connection, attempt to establish connection.
    case FeatureStatus::kEnabledButDisconnected:
      break;
  }

  if (previous_feature_status == FeatureStatus::kEnabledAndConnecting) {
    PA_LOG(WARNING) << "Scheduling connection to retry in: "
                    << retry_backoff_.GetTimeUntilRelease().InSeconds()
                    << " seconds.";

    retry_backoff_.InformOfRequest(/*succeeded=*/false);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ConnectionSchedulerImpl::ScheduleConnectionNow,
                       weak_ptr_factory_.GetWeakPtr()),
        retry_backoff_.GetTimeUntilRelease());
  } else {
    PA_LOG(VERBOSE) << "Feature status has been updated to "
                    << "kEnabledButDisconnected, scheduling connection now.";
    // Schedule connection now without a delay.
    ScheduleConnectionNow();
  }
}

void ConnectionSchedulerImpl::ClearBackoffAttempts() {
  // Remove all pending ScheduleConnectionNow() backoff attempts.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Reset the state of the backoff so that the next backoff retry starts at
  // the default initial delay.
  retry_backoff_.Reset();
}

void ConnectionSchedulerImpl::DisconnectAndClearBackoffAttempts() {
  ClearBackoffAttempts();

  // Disconnect existing connection or connection attempt.
  connection_manager_->Disconnect();
}

base::TimeDelta
ConnectionSchedulerImpl::GetCurrentBackoffDelayTimeForTesting() {
  return retry_backoff_.GetTimeUntilRelease();
}

int ConnectionSchedulerImpl::GetBackoffFailureCountForTesting() {
  return retry_backoff_.failure_count();
}

}  // namespace phonehub
}  // namespace ash
