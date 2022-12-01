// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/combined_access_setup_operation.h"

#include <array>

#include "base/check.h"
#include "base/containers/contains.h"

namespace ash {
namespace phonehub {

namespace {

// Status values which are considered "final" - i.e., once the status of an
// operation changes to one of these values, the operation has completed. These
// status values indicate either a success or a fatal error.
constexpr std::array<CombinedAccessSetupOperation::Status, 8>
    kOperationFinishedStatus{
        CombinedAccessSetupOperation::Status::kTimedOutConnecting,
        CombinedAccessSetupOperation::Status::kConnectionDisconnected,
        CombinedAccessSetupOperation::Status::kCompletedSuccessfully,
        CombinedAccessSetupOperation::Status::kProhibitedFromProvidingAccess,
        CombinedAccessSetupOperation::Status::kCompletedUserRejectedAllAccess,
        CombinedAccessSetupOperation::Status::kOperationFailedOrCancelled,
        CombinedAccessSetupOperation::Status::
            kCameraRollGrantedNotificationRejected,
        CombinedAccessSetupOperation::Status::
            kCameraRollRejectedNotificationGranted,
    };

}  // namespace

// static
bool CombinedAccessSetupOperation::IsFinalStatus(Status status) {
  return base::Contains(kOperationFinishedStatus, status);
}

CombinedAccessSetupOperation::CombinedAccessSetupOperation(
    Delegate* delegate,
    base::OnceClosure destructor_callback)
    : delegate_(delegate),
      destructor_callback_(std::move(destructor_callback)) {
  DCHECK(delegate_);
  DCHECK(destructor_callback_);
}

CombinedAccessSetupOperation::~CombinedAccessSetupOperation() {
  std::move(destructor_callback_).Run();
}

void CombinedAccessSetupOperation::NotifyCombinedStatusChanged(
    Status new_status) {
  current_status_ = new_status;

  delegate_->OnCombinedStatusChange(new_status);
}

std::ostream& operator<<(std::ostream& stream,
                         CombinedAccessSetupOperation::Status status) {
  switch (status) {
    case CombinedAccessSetupOperation::Status::kConnecting:
      stream << "[Connecting]";
      break;
    case CombinedAccessSetupOperation::Status::kTimedOutConnecting:
      stream << "[Timed out connecting]";
      break;
    case CombinedAccessSetupOperation::Status::kConnectionDisconnected:
      stream << "[Connection disconnected]";
      break;
    case CombinedAccessSetupOperation::Status::
        kSentMessageToPhoneAndWaitingForResponse:
      stream << "[Sent message to phone; waiting for response]";
      break;
    case CombinedAccessSetupOperation::Status::kCompletedSuccessfully:
      stream << "[Completed successfully]";
      break;
    case CombinedAccessSetupOperation::Status::kProhibitedFromProvidingAccess:
      stream << "[Prohibited from providing access]";
      break;
    case CombinedAccessSetupOperation::Status::kCompletedUserRejectedAllAccess:
      stream << "[User rejected to grant access]";
      break;
    case CombinedAccessSetupOperation::Status::kOperationFailedOrCancelled:
      stream << "[Operation failed or cancelled]";
      break;
    case CombinedAccessSetupOperation::Status::
        kCameraRollGrantedNotificationRejected:
      stream << "[User granted access to Camera Roll but rejected access to "
                "notification]";
      break;
    case CombinedAccessSetupOperation::Status::
        kCameraRollRejectedNotificationGranted:
      stream << "[User rejected access to Camera Roll but granted access to "
                "notification]";
      break;
  }

  return stream;
}

}  // namespace phonehub
}  // namespace ash
