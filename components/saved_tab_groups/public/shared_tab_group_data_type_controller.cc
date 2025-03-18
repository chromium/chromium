// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/shared_tab_group_data_type_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/data_sharing/public/data_type_controller/managed_account_precondition_checker.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"

namespace tab_groups {

SharedTabGroupDataTypeController::SharedTabGroupDataTypeController(
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager)
    : DataTypeController(syncer::SHARED_TAB_GROUP_DATA,
                         std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode)),
      precondition_checker_(
          sync_service,
          identity_manager,
          base::BindRepeating(&syncer::SyncService::DataTypePreconditionChanged,
                              base::Unretained(sync_service),
                              type())) {}

SharedTabGroupDataTypeController::~SharedTabGroupDataTypeController() = default;

syncer::DataTypeController::PreconditionState
SharedTabGroupDataTypeController::GetPreconditionState() const {
  return precondition_checker_.GetPreconditionState();
}

}  // namespace tab_groups
