// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_BACKEND_MIGRATOR_H_
#define COMPONENTS_SYNC_SERVICE_BACKEND_MIGRATOR_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/data_type_manager.h"

namespace syncer {

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

  BackendMigrator(const std::string& name,
                  DataTypeManager* manager,
                  const base::RepeatingClosure& reconfigure_callback,
                  const base::RepeatingClosure& migration_done_callback);

  BackendMigrator(const BackendMigrator&) = delete;
  BackendMigrator& operator=(const BackendMigrator&) = delete;

  virtual ~BackendMigrator();

  // Starts a sequence of events that will disable and reenable |types|.
  void MigrateTypes(DataTypeSet types);

  void AddMigrationObserver(MigrationObserver* observer);
  void RemoveMigrationObserver(MigrationObserver* observer);

  State state() const;

  // Called from SyncServiceImpl to notify us of configure done.
  // Note: We receive these notifications only when our state is not IDLE.
  void OnConfigureDone(const DataTypeManager::ConfigureResult& result);

  // Returns the types that are currently pending migration (if any).
  DataTypeSet GetPendingMigrationTypesForTest() const;

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
  const raw_ptr<DataTypeManager, DanglingUntriaged> manager_;

  const base::RepeatingClosure reconfigure_callback_;
  const base::RepeatingClosure migration_done_callback_;

  State state_;

  base::ObserverList<MigrationObserver>::Unchecked migration_observers_;

  DataTypeSet to_migrate_;

  base::WeakPtrFactory<BackendMigrator> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_BACKEND_MIGRATOR_H_
