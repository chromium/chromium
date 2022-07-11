// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_MODEL_TYPE_CONTROLLER_H_

#include "components/history/core/browser/sync/history_model_type_controller_helper.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_service_observer.h"

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace history {

class HistoryService;

// ModelTypeController for "history" data types - HISTORY and TYPED_URLS.
class HistoryModelTypeController : public syncer::ModelTypeController,
                                   public syncer::SyncServiceObserver {
 public:
  // `model_type` must be either HISTORY or TYPED_URLS.
  HistoryModelTypeController(syncer::ModelType model_type,
                             syncer::SyncService* sync_service,
                             HistoryService* history_service,
                             PrefService* pref_service);

  HistoryModelTypeController(const HistoryModelTypeController&) = delete;
  HistoryModelTypeController& operator=(const HistoryModelTypeController&) =
      delete;

  ~HistoryModelTypeController() override;

  // syncer::DataTypeController implementation.
  PreconditionState GetPreconditionState() const override;
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  void Stop(syncer::ShutdownReason shutdown_reason,
            StopCallback callback) override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  HistoryModelTypeControllerHelper helper_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_MODEL_TYPE_CONTROLLER_H_
