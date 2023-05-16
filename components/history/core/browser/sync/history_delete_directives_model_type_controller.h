// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_DELETE_DIRECTIVES_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_DELETE_DIRECTIVES_MODEL_TYPE_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "components/history/core/browser/sync/history_model_type_controller_helper.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/sync/service/syncable_service_based_model_type_controller.h"

class PrefService;

namespace syncer {
class ModelTypeStoreService;
class SyncService;
}  // namespace syncer

namespace history {

class HistoryService;

// A controller for delete directives, which cannot sync when full encryption
// is enabled.
class HistoryDeleteDirectivesModelTypeController
    : public syncer::SyncableServiceBasedModelTypeController,
      public syncer::SyncServiceObserver {
 public:
  // `sync_service`, `history_service`, and `pref_service` must not be null and
  // must outlive this object.
  HistoryDeleteDirectivesModelTypeController(
      const base::RepeatingClosure& dump_stack,
      syncer::SyncService* sync_service,
      syncer::ModelTypeStoreService* model_type_store_service,
      HistoryService* history_service,
      PrefService* pref_service);

  HistoryDeleteDirectivesModelTypeController(
      const HistoryDeleteDirectivesModelTypeController&) = delete;
  HistoryDeleteDirectivesModelTypeController& operator=(
      const HistoryDeleteDirectivesModelTypeController&) = delete;

  ~HistoryDeleteDirectivesModelTypeController() override;

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  void Stop(syncer::SyncStopMetadataFate fate, StopCallback callback) override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  history::HistoryModelTypeControllerHelper helper_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_DELETE_DIRECTIVES_MODEL_TYPE_CONTROLLER_H_
