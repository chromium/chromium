// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_DATA_TYPE_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "components/history/core/browser/sync/history_data_type_controller_helper.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

class PrefService;

namespace history {

class HistoryService;

// DataTypeController for syncer::HISTORY.
class HistoryDataTypeController : public syncer::DataTypeController,
                                  public syncer::SyncServiceObserver {
 public:
  HistoryDataTypeController(syncer::SyncService* sync_service,
                            HistoryService* history_service,
                            PrefService* pref_service);

  HistoryDataTypeController(const HistoryDataTypeController&) = delete;
  HistoryDataTypeController& operator=(const HistoryDataTypeController&) =
      delete;

  ~HistoryDataTypeController() override;

  // syncer::DataTypeController implementation.
  PreconditionState GetPreconditionState(
      const PreconditionContext& context) const override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  HistoryDataTypeControllerHelper helper_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};

  const raw_ptr<HistoryService> history_service_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_DATA_TYPE_CONTROLLER_H_
