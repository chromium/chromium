// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_CONTACT_INFO_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_CONTACT_INFO_MODEL_TYPE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/model_type_controller.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace autofill {

class ContactInfoModelTypeController
    : public syncer::ModelTypeController,
      public syncer::SyncServiceObserver,
      public signin::IdentityManager::Observer {
 public:
  ContactInfoModelTypeController(
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_transport_mode,
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager);
  ~ContactInfoModelTypeController() override;

  ContactInfoModelTypeController(const ContactInfoModelTypeController&) =
      delete;
  ContactInfoModelTypeController& operator=(
      const ContactInfoModelTypeController&) = delete;

  // ModelTypeController overrides.
  PreconditionState GetPreconditionState() const override;

  // SyncServiceObserver overrides.
  void OnStateChanged(syncer::SyncService* sync) override;

  // IdentityManager::Observer overrides.
  void OnRefreshTokensLoaded() override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

 private:
  // Called by the `managed_status_finder_` when it determines the account type.
  void AccountTypeDetermined();

  const raw_ptr<syncer::SyncService> sync_service_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  const raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};
  std::unique_ptr<signin::AccountManagedStatusFinder> managed_status_finder_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_CONTACT_INFO_MODEL_TYPE_CONTROLLER_H_
