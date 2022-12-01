// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/notification_access_setup_operation.h"

#include <array>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"

namespace ash {
namespace phonehub {

namespace {

// Status values which are considered "final" - i.e., once the status of an
// operation changes to one of these values, the operation has completed. These
// status values indicate either a success or a fatal error.
constexpr std::array<NotificationAccessSetupOperation::Status, 4>
    kOperationFinishedStatus{
        NotificationAccessSetupOperation::Status::kTimedOutConnecting,
        NotificationAccessSetupOperation::Status::kConnectionDisconnected,
        NotificationAccessSetupOperation::Status::kCompletedSuccessfully,
        NotificationAccessSetupOperation::Status::
            kProhibitedFromProvidingAccess,
    };

// Used for metrics; do not change.
constexpr size_t kNumSetupDurationHistogramBuckets = 50;
constexpr base::TimeDelta kSetupDurationHistogramMinTime = base::Seconds(1);
constexpr base::TimeDelta kSetupDurationHistogramMaxTime = base::Minutes(10);

}  // namespace

// static
bool NotificationAccessSetupOperation::IsFinalStatus(Status status) {
  return base::Contains(kOperationFinishedStatus, status);
}

NotificationAccessSetupOperation::NotificationAccessSetupOperation(
    Delegate* delegate,
    base::OnceClosure destructor_callback)
    : delegate_(delegate),
      destructor_callback_(std::move(destructor_callback)) {
  DCHECK(delegate_);
  DCHECK(destructor_callback_);
}

NotificationAccessSetupOperation::~NotificationAccessSetupOperation() {
  if (current_status_) {
    base::UmaHistogramEnumeration("PhoneHub.NotificationAccessSetup.LastStatus",
                                  *current_status_);
  }

  std::move(destructor_callback_).Run();
}

void NotificationAccessSetupOperation::NotifyNotificationStatusChanged(
    Status new_status) {
  base::UmaHistogramEnumeration("PhoneHub.NotificationAccessSetup.AllStatuses",
                                new_status);
  if (new_status == Status::kCompletedSuccessfully) {
    base::UmaHistogramCustomTimes(
        "PhoneHub.NotificationAccessSetup.SuccessfulSetupDuration",
        base::TimeTicks::Now() - start_timestamp_,
        kSetupDurationHistogramMinTime, kSetupDurationHistogramMaxTime,
        kNumSetupDurationHistogramBuckets);
  }
  current_status_ = new_status;

  delegate_->OnNotificationStatusChange(new_status);
}

std::ostream& operator<<(std::ostream& stream,
                         NotificationAccessSetupOperation::Status status) {
  switch (status) {
    case NotificationAccessSetupOperation::Status::kConnecting:
      stream << "[Connecting]";
      break;
    case NotificationAccessSetupOperation::Status::kTimedOutConnecting:
      stream << "[Timed out connecting]";
      break;
    case NotificationAccessSetupOperation::Status::kConnectionDisconnected:
      stream << "[Connection disconnected]";
      break;
    case NotificationAccessSetupOperation::Status::
        kSentMessageToPhoneAndWaitingForResponse:
      stream << "[Sent message to phone; waiting for response]";
      break;
    case NotificationAccessSetupOperation::Status::kCompletedSuccessfully:
      stream << "[Completed successfully]";
      break;
    case NotificationAccessSetupOperation::Status::
        kProhibitedFromProvidingAccess:
      stream << "[Prohibited from providing access]";
      break;
  }

  return stream;
}

}  // namespace phonehub
}  // namespace ash
