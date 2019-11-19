// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_BACKEND_MIGRATOR_H_
#define COMPONENTS_SYNC_DRIVER_BACKEND_MIGRATOR_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_manager.h"

namespace syncer {

struct UserShare;

// Interface for anything that wants to know when the migrator's state
// changes.
class MigrationObserver {
 public:
  virtual void OnMigrationStateChange() = 0;

 protected:
  virtual ~MigrationObserver();
};

// A class to perform migration of a datatype pursuant to the 'MIGRATION_DONE'
// code in the sync protocol definition (protocol/sync.proto).
class BackendMigrator {
 public:
  enum State {
    IDLE,
    WAITING_TO_START,  // Waiting for previous configuration to finish.
    DISABLING_TYPES,   // Exit criteria: OnConfigureDone for enabled types
                       // _excluding_ |to_migrate_| and empty download progress
                       // markers for types in |to_migrate_|.
    REENABLING_TYPES,  // Exit criteria: OnConfigureDone for enabled types.
  };

  // TODO(akalin): Remove the dependency on |user_share|.
  BackendMigrator(const std::string& name,
                  UserShare* user_share,
                  DataTypeManager* manager,
                  const base::RepeatingClosure& reconfigure_callback,
                  const base::RepeatingClosure& migration_done_callback);
  virtual ~BackendMigrator();

  // Starts a sequence of events that will disable and reenable |types|.
  void MigrateTypes(ModelTypeSet types);

  void AddMigrationObserver(MigrationObserver* observer);
  void RemoveMigrationObserver(MigrationObserver* observer);

  State state() const;

  // Called from ProfileSyncService to notify us of configure done.
  // Note: We receive these notifications only when our state is not IDLE.
  void OnConfigureDone(const DataTypeManager::ConfigureResult& result);

  // Returns the types that are currently pending migration (if any).
  ModelTypeSet GetPendingMigrationTypesForTest() const;

 private:
  void ChangeState(State new_state);

  // Must be called only in state WAITING_TO_START.  If ready to
  // start, meaning the data type manager is configured, calls
  // RestartMigration() and returns true.  Otherwise, does nothing and
  // returns false.
  bool TryStart();

  // Restarts migration, interrupting any existing migration.
  void RestartMigration();

  // Called by OnConfigureDone().
  void OnConfigureDoneImpl(const DataTypeManager::ConfigureResult& result);

  const std::string name_;
  UserShare* user_share_;
  DataTypeManager* manager_;

  const base::RepeatingClosure reconfigure_callback_;
  const base::RepeatingClosure migration_done_callback_;

  State state_;

  base::ObserverList<MigrationObserver>::Unchecked migration_observers_;

  ModelTypeSet to_migrate_;

  base::WeakPtrFactory<BackendMigrator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BackendMigrator);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_BACKEND_MIGRATOR_H_
