// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/data_type_controller/collaboration_group_data_type_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/collaboration/public/data_type_controller/collaboration_service_precondition_checker.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"

namespace collaboration {

CollaborationGroupDataTypeController::CollaborationGroupDataTypeController(
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode,
    syncer::SyncService* sync_service,
    collaboration::CollaborationService* collaboration_service)
    : DataTypeController(syncer::COLLABORATION_GROUP,
                         std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode)),
      precondition_checker_(
          collaboration_service,
          base::BindRepeating(&syncer::SyncService::DataTypePreconditionChanged,
                              base::Unretained(sync_service),
                              type())) {}

CollaborationGroupDataTypeController::~CollaborationGroupDataTypeController() =
    default;

syncer::DataTypeController::PreconditionState
CollaborationGroupDataTypeController::GetPreconditionState() const {
  return precondition_checker_.GetPreconditionState();
}

}  // namespace collaboration
