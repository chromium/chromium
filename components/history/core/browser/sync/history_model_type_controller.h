// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_MODEL_TYPE_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "components/history/core/browser/sync/history_model_type_controller_helper.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

class PrefService;

namespace signin {
class AccountManagedStatusFinder;
class IdentityManager;
}  // namespace signin

namespace history {

class HistoryService;

// ModelTypeController for "history" data types - HISTORY and TYPED_URLS.
class HistoryModelTypeController : public syncer::ModelTypeController,
                                   public syncer::SyncServiceObserver {
 public:
  // `model_type` must be either HISTORY or TYPED_URLS.
  HistoryModelTypeController(syncer::ModelType model_type,
                             syncer::SyncService* sync_service,
                             signin::IdentityManager* identity_manager,
                             HistoryService* history_service,
                             PrefService* pref_service);

  HistoryModelTypeController(const HistoryModelTypeController&) = delete;
  HistoryModelTypeController& operator=(const HistoryModelTypeController&) =
      delete;

  ~HistoryModelTypeController() override;

  // syncer::DataTypeController implementation.
  PreconditionState GetPreconditionState() const override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  void AccountTypeDetermined();

  HistoryModelTypeControllerHelper helper_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};

  const raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<signin::AccountManagedStatusFinder> managed_status_finder_;

  const raw_ptr<HistoryService> history_service_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_MODEL_TYPE_CONTROLLER_H_
