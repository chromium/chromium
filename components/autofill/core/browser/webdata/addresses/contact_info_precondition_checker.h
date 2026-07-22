// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_PRECONDITION_CHECKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_PRECONDITION_CHECKER_H_

#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace autofill {

// Helper class to determine whether a user is eligible for the CONTACT_INFO
// sync data type. This is needed to disable the data type for unsupported
// users in the data type controller. It is also needed to hide the opt-out
// option in the settings for unsupported users.
class ContactInfoPreconditionChecker : public syncer::SyncServiceObserver {
 public:
  // `on_precondition_changed` is called whenever the result of
  // `GetPreconditionState()` has possibly changed.
  ContactInfoPreconditionChecker(
      syncer::SyncService* sync_service,
      base::RepeatingClosure on_precondition_changed);
  ~ContactInfoPreconditionChecker() override;

  syncer::DataTypeController::PreconditionState GetPreconditionState(
      const syncer::DataTypeController::PreconditionContext& context) const;

  // SyncServiceObserver overrides.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  // Returns the SyncService, or `nullptr` after `OnSyncShutdown()`.
  const syncer::SyncService* GetSyncService() const;

  const base::RepeatingClosure on_precondition_changed_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_PRECONDITION_CHECKER_H_
