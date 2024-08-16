// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "components/history/core/browser/sync/history_data_type_controller_helper.h"
#include "components/sync/service/data_type_controller.h"

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace sync_sessions {

class SessionDataTypeController : public syncer::DataTypeController {
 public:
  SessionDataTypeController(syncer::SyncService* sync_service,
                            PrefService* pref_service,
                            std::unique_ptr<syncer::DataTypeControllerDelegate>
                                delegate_for_full_sync_mode,
                            std::unique_ptr<syncer::DataTypeControllerDelegate>
                                delegate_for_transport_mode);

  SessionDataTypeController(const SessionDataTypeController&) = delete;
  SessionDataTypeController& operator=(const SessionDataTypeController&) =
      delete;

  ~SessionDataTypeController() override;

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;

 private:
  history::HistoryDataTypeControllerHelper helper_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_DATA_TYPE_CONTROLLER_H_
