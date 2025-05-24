// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/data_type_controller/collaboration_service_precondition_checker.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/data_sharing/public/features.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_user_settings.h"
#include "ui/base/device_form_factor.h"

namespace collaboration {
using PreconditionState = syncer::DataTypeController::PreconditionState;

CollaborationServicePreconditionChecker::
    CollaborationServicePreconditionChecker(
        CollaborationService* collaboration_service,
        base::RepeatingClosure on_precondition_changed)
    : collaboration_service_(CHECK_DEREF(collaboration_service)),
      on_precondition_changed_(std::move(on_precondition_changed)) {
  collaboration_service_->AddObserver(this);
}

CollaborationServicePreconditionChecker::
    ~CollaborationServicePreconditionChecker() {
  collaboration_service_->RemoveObserver(this);
}

void CollaborationServicePreconditionChecker::OnServiceStatusChanged(
    const CollaborationService::Observer::ServiceStatusUpdate& update) {
  on_precondition_changed_.Run();
}

PreconditionState
CollaborationServicePreconditionChecker::GetPreconditionState() const {
  ServiceStatus service_status = collaboration_service_->GetServiceStatus();
  switch (service_status.collaboration_status) {
    case CollaborationStatus::kDisabled:
    case CollaborationStatus::kDisabledPending:
      return PreconditionState::kMustStopAndKeepData;
    case CollaborationStatus::kDisabledForPolicy:
      return PreconditionState::kMustStopAndClearData;
    case CollaborationStatus::kAllowedToJoin:
    case CollaborationStatus::kEnabledJoinOnly:
    case CollaborationStatus::kEnabledCreateAndJoin:
      return PreconditionState::kPreconditionsMet;
    default:
      NOTREACHED();
  }
}

}  // namespace collaboration
