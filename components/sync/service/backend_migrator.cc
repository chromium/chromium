// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/backend_migrator.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"

namespace syncer {

MigrationObserver::~MigrationObserver() = default;

BackendMigrator::BackendMigrator(
    const std::string& name,
    DataTypeManager* manager,
    const base::RepeatingClosure& reconfigure_callback,
    const base::RepeatingClosure& migration_done_callback)
    : name_(name),
      manager_(manager),
      reconfigure_callback_(reconfigure_callback),
      migration_done_callback_(migration_done_callback),
      state_(IDLE) {
  DCHECK(!reconfigure_callback_.is_null());
  DCHECK(!migration_done_callback_.is_null());
}

BackendMigrator::~BackendMigrator() = default;

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncer threads involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

void BackendMigrator::MigrateTypes(DataTypeSet types) {
  const DataTypeSet old_to_migrate = to_migrate_;
  to_migrate_.PutAll(types);
  SDVLOG(1) << "MigrateTypes called with " << DataTypeSetToDebugString(types)
            << ", old_to_migrate = " << DataTypeSetToDebugString(old_to_migrate)
            << ", to_migrate_ = " << DataTypeSetToDebugString(to_migrate_);
  if (old_to_migrate == to_migrate_) {
    SDVLOG(1) << "MigrateTypes called with no new types; ignoring";
    return;
  }

  if (state_ == IDLE) {
    ChangeState(WAITING_TO_START);
  }

  if (state_ == WAITING_TO_START) {
    if (!TryStart()) {
      SDVLOG(1) << "Manager not configured; waiting";
    }
    return;
  }

  DCHECK_GT(state_, WAITING_TO_START);
  // If we're already migrating, interrupt the current migration.
  RestartMigration();
}

void BackendMigrator::AddMigrationObserver(MigrationObserver* observer) {
  migration_observers_.AddObserver(observer);
}

void BackendMigrator::RemoveMigrationObserver(MigrationObserver* observer) {
  migration_observers_.RemoveObserver(observer);
}

void BackendMigrator::ChangeState(State new_state) {
  state_ = new_state;
  for (MigrationObserver& observer : migration_observers_) {
    observer.OnMigrationStateChange();
  }
}

bool BackendMigrator::TryStart() {
  DCHECK_EQ(state_, WAITING_TO_START);
  if (manager_->state() == DataTypeManager::CONFIGURED) {
    RestartMigration();
    return true;
  }
  return false;
}

void BackendMigrator::RestartMigration() {
  // We'll now disable any running types that need to be migrated.
  ChangeState(DISABLING_TYPES);
  SDVLOG(1) << "BackendMigrator disabling types "
            << DataTypeSetToDebugString(to_migrate_);

  manager_->PurgeForMigration(to_migrate_);
}

void BackendMigrator::OnConfigureDone(
    const DataTypeManager::ConfigureResult& result) {
  if (state_ == IDLE) {
    return;
  }

  // |manager_|'s methods aren't re-entrant, and we're notified from
  // them, so post a task to avoid problems.
  SDVLOG(1) << "Posting OnConfigureDoneImpl";
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BackendMigrator::OnConfigureDoneImpl,
                                weak_ptr_factory_.GetWeakPtr(), result));
}

void BackendMigrator::OnConfigureDoneImpl(
    const DataTypeManager::ConfigureResult& result) {
  SDVLOG(1) << "OnConfigureDone with requested types "
            << DataTypeSetToDebugString(result.requested_types) << ", status "
            << result.status
            << ", and to_migrate_ = " << DataTypeSetToDebugString(to_migrate_);
  if (state_ == WAITING_TO_START) {
    if (!TryStart()) {
      SDVLOG(1) << "Manager still not configured; still waiting";
    }
    return;
  }

  DCHECK_GT(state_, WAITING_TO_START);

  const DataTypeSet intersection =
      Intersection(result.requested_types, to_migrate_);
  // This intersection check is to determine if our disable request
  // was interrupted by a user changing preferred types.
  if (state_ == DISABLING_TYPES && !intersection.empty()) {
    SDVLOG(1) << "Disable request interrupted by user changing types";
    RestartMigration();
    return;
  }

  if (result.status != DataTypeManager::OK) {
    // If this fails, and we're disabling types, a type may or may not be
    // disabled until the user restarts the browser.  If this wasn't an abort,
    // any failure will be reported as an unrecoverable error to the UI. If it
    // was an abort, then typically things are shutting down anyway. There isn't
    // much we can do in any case besides wait until a restart to try again.
    // The server will send down MIGRATION_DONE again for types needing
    // migration as the type will still be enabled on restart.
    SLOG(WARNING) << "Unable to migrate, configuration failed!";
    ChangeState(IDLE);
    to_migrate_.Clear();
    return;
  }

  if (state_ == DISABLING_TYPES) {
    DataTypeSet stopped_types = manager_->GetStoppedDataTypesExcludingNigori();
    // NIGORI does not have a controller and is hence not managed by
    // DataTypeManager, which means it's never returned in
    // GetStoppedDataTypesExcludingNigori(). Luckily, there's no need to wait
    // until NIGORI is purged, because that takes effect immediately.
    // TODO(crbug.com/40154783): try to find better way to implement this logic.
    stopped_types.Put(NIGORI);

    if (!stopped_types.HasAll(to_migrate_)) {
      SLOG(WARNING) << "Set of stopped types: "
                    << DataTypeSetToDebugString(stopped_types)
                    << " does not contain types to migrate: "
                    << DataTypeSetToDebugString(to_migrate_)
                    << "; not re-enabling yet due to "
                    << DataTypeSetToDebugString(
                           Difference(to_migrate_, stopped_types));
      return;
    }

    ChangeState(REENABLING_TYPES);
    SDVLOG(1) << "BackendMigrator triggering reconfiguration";
    reconfigure_callback_.Run();
  } else if (state_ == REENABLING_TYPES) {
    // We're done!
    ChangeState(IDLE);

    SDVLOG(1) << "BackendMigrator: Migration complete for: "
              << DataTypeSetToDebugString(to_migrate_);
    to_migrate_.Clear();
    migration_done_callback_.Run();
  }
}

BackendMigrator::State BackendMigrator::state() const {
  return state_;
}

DataTypeSet BackendMigrator::GetPendingMigrationTypesForTest() const {
  return to_migrate_;
}

#undef SDVLOG

#undef SLOG

}  // namespace syncer
