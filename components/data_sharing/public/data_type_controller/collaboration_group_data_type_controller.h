// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_DATA_TYPE_CONTROLLER_COLLABORATION_GROUP_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_DATA_TYPE_CONTROLLER_COLLABORATION_GROUP_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "components/data_sharing/public/data_type_controller/managed_account_precondition_checker.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service.h"

namespace signin {
class IdentityManager;
}

namespace syncer {
class DataTypeControllerDelegate;
class SyncService;
}  // namespace syncer

namespace data_sharing {

class CollaborationGroupDataTypeController : public syncer::DataTypeController {
 public:
  CollaborationGroupDataTypeController(
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode,
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager);
  ~CollaborationGroupDataTypeController() override;

  CollaborationGroupDataTypeController(
      const CollaborationGroupDataTypeController&) = delete;
  CollaborationGroupDataTypeController& operator=(
      const CollaborationGroupDataTypeController&) = delete;

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;

 private:
  ManagedAccountPreconditionChecker precondition_checker_;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_DATA_TYPE_CONTROLLER_COLLABORATION_GROUP_DATA_TYPE_CONTROLLER_H_
