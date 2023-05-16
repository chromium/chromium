// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "components/history/core/browser/sync/history_model_type_controller_helper.h"
#include "components/sync/service/model_type_controller.h"

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace sync_sessions {

class SessionModelTypeController : public syncer::ModelTypeController {
 public:
  SessionModelTypeController(
      syncer::SyncService* sync_service,
      PrefService* pref_service,
      std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate);

  SessionModelTypeController(const SessionModelTypeController&) = delete;
  SessionModelTypeController& operator=(const SessionModelTypeController&) =
      delete;

  ~SessionModelTypeController() override;

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;

 private:
  history::HistoryModelTypeControllerHelper helper_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_MODEL_TYPE_CONTROLLER_H_
