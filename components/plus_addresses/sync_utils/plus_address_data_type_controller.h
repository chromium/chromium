// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_SYNC_UTILS_PLUS_ADDRESS_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_PLUS_ADDRESSES_SYNC_UTILS_PLUS_ADDRESS_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/variations/service/google_groups_manager.h"

class GoogleGroupsManager;

namespace signin {
class AccountManagedStatusFinder;
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace plus_addresses {

// Shared data type controller for PLUS_ADDRESS and PLUS_ADDRESS_SETTING.
// It is responsible for disabling the types when the feature is not enabled or
// the user type not supported.
// Tested by the sync integration tests.
class PlusAddressDataTypeController : public syncer::DataTypeController,
                                       public syncer::SyncServiceObserver {
 public:
  PlusAddressDataTypeController(
      syncer::DataType type,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode,
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager,
      GoogleGroupsManager* google_groups_manager);
  ~PlusAddressDataTypeController() override;

  // DataTypeController:
  PreconditionState GetPreconditionState() const override;

  // SyncServiceObserver:
  void OnStateChanged(syncer::SyncService*) override;

 private:
  void RecreateManagedStatusFinder();

  // Called by the `managed_status_finder_` when it determined the account type.
  void OnAccountTypeDetermined();

  const raw_ptr<syncer::SyncService> sync_service_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  const raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<signin::AccountManagedStatusFinder> managed_status_finder_;
  const raw_ref<GoogleGroupsManager> google_groups_manager_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_SYNC_UTILS_PLUS_ADDRESS_DATA_TYPE_CONTROLLER_H_
