// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_MODEL_TYPE_CONTROLLER_HELPER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_MODEL_TYPE_CONTROLLER_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_controller.h"

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace history {

// Helper class for implementing history-related ModelTypeControllers. It
// implements the pref/policy kSavingBrowserHistoryDisabled: It calls
// SyncService::DataTypePreconditionChanged() when the pref changes.
// DataTypeControllers using this helper must call its GetPreconditionState().
class HistoryModelTypeControllerHelper {
 public:
  HistoryModelTypeControllerHelper(syncer::ModelType model_type,
                                   syncer::SyncService* sync_service,
                                   PrefService* pref_service);

  HistoryModelTypeControllerHelper(const HistoryModelTypeControllerHelper&) =
      delete;
  HistoryModelTypeControllerHelper& operator=(
      const HistoryModelTypeControllerHelper&) = delete;

  ~HistoryModelTypeControllerHelper();

  // Must be called from DataTypeController::GetPreconditionState().
  syncer::DataTypeController::PreconditionState GetPreconditionState() const;

  syncer::SyncService* sync_service() const { return sync_service_; }

 private:
  void OnSavingBrowserHistoryDisabledChanged();

  const syncer::ModelType model_type_;
  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<PrefService> pref_service_;

  PrefChangeRegistrar pref_registrar_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_MODEL_TYPE_CONTROLLER_HELPER_H_
