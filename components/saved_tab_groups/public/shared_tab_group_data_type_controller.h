// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SHARED_TAB_GROUP_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SHARED_TAB_GROUP_DATA_TYPE_CONTROLLER_H_

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

namespace tab_groups {

class SharedTabGroupDataTypeController : public syncer::DataTypeController {
 public:
  SharedTabGroupDataTypeController(
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode,
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager);
  ~SharedTabGroupDataTypeController() override;

  SharedTabGroupDataTypeController(const SharedTabGroupDataTypeController&) =
      delete;
  SharedTabGroupDataTypeController& operator=(
      const SharedTabGroupDataTypeController&) = delete;

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;

 private:
  data_sharing::ManagedAccountPreconditionChecker precondition_checker_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_SHARED_TAB_GROUP_DATA_TYPE_CONTROLLER_H_
