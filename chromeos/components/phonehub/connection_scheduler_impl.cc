// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/connection_scheduler_impl.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/connection_manager.h"
#include "chromeos/components/phonehub/feature_status.h"

namespace chromeos {
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
    ConnectionManager* connection_manager,
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

  connection_manager_->AttemptConnection();
}

void ConnectionSchedulerImpl::OnFeatureStatusChanged() {
  const FeatureStatus previous_feature_status = current_feature_status_;
  current_feature_status_ = feature_status_provider_->GetStatus();

  switch (current_feature_status_) {
    // The following states indicate either the feature state of the devices
    // changed or if a connection is established between the devices. In the
    // case where the feature state has been changed, we do not want to
    // schedule a new connection attempt until the devices are available to
    // connect. If a connection is established, we also do not want to schedule
    // a new connection. Reset the backoff and return early.
    case FeatureStatus::kNotEligibleForFeature:
      FALLTHROUGH;
    case FeatureStatus::kEligiblePhoneButNotSetUp:
      FALLTHROUGH;
    case FeatureStatus::kPhoneSelectedAndPendingSetup:
      FALLTHROUGH;
    case FeatureStatus::kDisabled:
      FALLTHROUGH;
    case FeatureStatus::kUnavailableBluetoothOff:
      FALLTHROUGH;
    case FeatureStatus::kEnabledAndConnected:
      ClearBackoffAttempts();
      return;

    // Connection in progress, waiting for the next status update.
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
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
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

base::TimeDelta
ConnectionSchedulerImpl::GetCurrentBackoffDelayTimeForTesting() {
  return retry_backoff_.GetTimeUntilRelease();
}

int ConnectionSchedulerImpl::GetBackoffFailureCountForTesting() {
  return retry_backoff_.failure_count();
}

}  // namespace phonehub
}  // namespace chromeos
