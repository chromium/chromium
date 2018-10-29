// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_DATA_TYPE_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "components/sync/driver/async_directory_type_controller.h"
#include "components/sync/driver/sync_service_observer.h"

namespace password_manager {
class PasswordStore;
}

namespace syncer {
class SyncClient;
}

namespace browser_sync {

// A class that manages the startup and shutdown of password sync.
class PasswordDataTypeController : public syncer::AsyncDirectoryTypeController,
                                   public syncer::SyncServiceObserver {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  PasswordDataTypeController(
      const base::RepeatingClosure& dump_stack,
      syncer::SyncClient* sync_client,
      const base::RepeatingClosure& state_changed_callback,
      const scoped_refptr<password_manager::PasswordStore>& password_store);
  ~PasswordDataTypeController() override;

 protected:
  // AsyncDirectoryTypeController interface.
  bool PostTaskOnModelThread(const base::Location& from_here,
                             const base::RepeatingClosure& task) override;
  bool StartModels() override;
  void StopModels() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  syncer::SyncClient* const sync_client_;
  const base::RepeatingClosure state_changed_callback_;
  scoped_refptr<password_manager::PasswordStore> password_store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_DATA_TYPE_CONTROLLER_H_
