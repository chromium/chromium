// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_DATA_TYPE_CONTROLLER_SHARED_TAB_GROUP_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_COLLABORATION_PUBLIC_DATA_TYPE_CONTROLLER_SHARED_TAB_GROUP_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "components/collaboration/public/data_type_controller/collaboration_service_precondition_checker.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service.h"

namespace collaboration {
class CollaborationService;
}  // namespace collaboration

namespace syncer {
class DataTypeControllerDelegate;
class SyncService;
}  // namespace syncer

namespace collaboration {

class SharedTabGroupDataTypeController : public syncer::DataTypeController {
 public:
  SharedTabGroupDataTypeController(
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode,
      syncer::SyncService* sync_service,
      collaboration::CollaborationService* collaboration_service);
  ~SharedTabGroupDataTypeController() override;

  SharedTabGroupDataTypeController(const SharedTabGroupDataTypeController&) =
      delete;
  SharedTabGroupDataTypeController& operator=(
      const SharedTabGroupDataTypeController&) = delete;

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;

 private:
  collaboration::CollaborationServicePreconditionChecker precondition_checker_;
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_PUBLIC_DATA_TYPE_CONTROLLER_SHARED_TAB_GROUP_DATA_TYPE_CONTROLLER_H_
