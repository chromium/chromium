// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SHARED_TAB_GROUP_PRECONDITION_CHECKER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SHARED_TAB_GROUP_PRECONDITION_CHECKER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace tab_groups {

// Helper class to determine whether a user is eligible for syncing the
// SHARED_TAB_GROUP_DATA sync data type. This is needed to disable the data type
// for unsupported users such as enterprise users in the data type controller.
class SharedTabGroupPreconditionChecker : public syncer::SyncServiceObserver {
 public:
  // `on_precondition_changed` is called whenever the result of
  // `GetPreconditionState()` has possibly changed.
  SharedTabGroupPreconditionChecker(
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager,
      base::RepeatingClosure on_precondition_changed);
  ~SharedTabGroupPreconditionChecker() override;

  syncer::DataTypeController::PreconditionState GetPreconditionState() const;

  // SyncServiceObserver overrides.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  // Called by the `managed_status_finder_` when it determines the account type.
  void AccountTypeDetermined();

  const raw_ref<syncer::SyncService> sync_service_;
  const raw_ref<signin::IdentityManager> identity_manager_;
  const base::RepeatingClosure on_precondition_changed_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  std::unique_ptr<signin::AccountManagedStatusFinder> managed_status_finder_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SHARED_TAB_GROUP_PRECONDITION_CHECKER_H_
