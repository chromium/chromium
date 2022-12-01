// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/feature_setup_connection_operation.h"

#include <array>

#include "base/check.h"
#include "base/containers/contains.h"

namespace ash {
namespace phonehub {

namespace {

constexpr std::array<FeatureSetupConnectionOperation::Status, 3>
    kOperationFinishedStatus{
        FeatureSetupConnectionOperation::Status::kTimedOutConnecting,
        FeatureSetupConnectionOperation::Status::kConnectionLost,
        FeatureSetupConnectionOperation::Status::kCompletedSuccessfully,
    };

}  // namespace

// static
bool FeatureSetupConnectionOperation::IsFinalStatus(Status status) {
  return base::Contains(kOperationFinishedStatus, status);
}

FeatureSetupConnectionOperation::FeatureSetupConnectionOperation(
    Delegate* delegate,
    base::OnceClosure destructor_callback)
    : delegate_(delegate),
      destructor_callback_(std::move(destructor_callback)) {
  DCHECK(delegate_);
  DCHECK(destructor_callback_);
}

FeatureSetupConnectionOperation::~FeatureSetupConnectionOperation() {
  std::move(destructor_callback_).Run();
}

void FeatureSetupConnectionOperation::NotifyFeatureSetupConnectionStatusChanged(
    Status new_status) {
  current_status_ = new_status;

  delegate_->OnFeatureSetupConnectionStatusChange(new_status);
}

std::ostream& operator<<(std::ostream& stream,
                         FeatureSetupConnectionOperation::Status status) {
  switch (status) {
    case FeatureSetupConnectionOperation::Status::kConnecting:
      stream << "[Connecting]";
      break;
    case FeatureSetupConnectionOperation::Status::kTimedOutConnecting:
      stream << "[Timed out connecting]";
      break;
    case FeatureSetupConnectionOperation::Status::kConnectionLost:
      stream << "[Connection disconnected]";
      break;
    case FeatureSetupConnectionOperation::Status::kCompletedSuccessfully:
      stream << "[Operation finished]";
      break;
    case FeatureSetupConnectionOperation::Status::kConnected:
      stream << "[Connection connected]";
      break;
  }

  return stream;
}

}  // namespace phonehub
}  // namespace ash
