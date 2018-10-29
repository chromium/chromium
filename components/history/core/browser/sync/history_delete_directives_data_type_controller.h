// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_DELETE_DIRECTIVES_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_DELETE_DIRECTIVES_DATA_TYPE_CONTROLLER_H_

#include "base/macros.h"
#include "components/sync/driver/async_directory_type_controller.h"
#include "components/sync/driver/sync_service_observer.h"

namespace browser_sync {

// A controller for delete directives, which cannot sync when full encryption
// is enabled.
class HistoryDeleteDirectivesDataTypeController
    : public syncer::AsyncDirectoryTypeController,
      public syncer::SyncServiceObserver {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  HistoryDeleteDirectivesDataTypeController(const base::Closure& dump_stack,
                                            syncer::SyncClient* sync_client);
  ~HistoryDeleteDirectivesDataTypeController() override;

  // AsyncDirectoryTypeController override.
  bool ReadyForStart() const override;
  bool StartModels() override;
  void StopModels() override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  // Triggers a SingleDataTypeUnrecoverable error and returns true if the
  // type is no longer ready, else does nothing and returns false.
  bool DisableTypeIfNecessary();

  syncer::SyncClient* sync_client_;

  DISALLOW_COPY_AND_ASSIGN(HistoryDeleteDirectivesDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_DELETE_DIRECTIVES_DATA_TYPE_CONTROLLER_H_
