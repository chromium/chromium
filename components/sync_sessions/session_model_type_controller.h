// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/model_type_controller.h"

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace sync_sessions {

// Overrides LoadModels to check if history sync is allowed by policy.
class SessionModelTypeController : public syncer::ModelTypeController {
 public:
  SessionModelTypeController(
      syncer::SyncService* sync_service,
      PrefService* pref_service,
      std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate);
  ~SessionModelTypeController() override;

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;

 private:
  void OnSavingBrowserHistoryPrefChanged();

  syncer::SyncService* const sync_service_;
  PrefService* const pref_service_;

  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SessionModelTypeController);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_MODEL_TYPE_CONTROLLER_H_
