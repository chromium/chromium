// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/backend_migrator.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/read_transaction.h"

namespace syncer {

MigrationObserver::~MigrationObserver() {}

BackendMigrator::BackendMigrator(
    const std::string& name,
    UserShare* user_share,
    DataTypeManager* manager,
    const base::RepeatingClosure& reconfigure_callback,
    const base::RepeatingClosure& migration_done_callback)
    : name_(name),
      user_share_(user_share),
      manager_(manager),
      reconfigure_callback_(reconfigure_callback),
      migration_done_callback_(migration_done_callback),
      state_(IDLE) {
  DCHECK(!reconfigure_callback_.is_null());
  DCHECK(!migration_done_callback_.is_null());
}

BackendMigrator::~BackendMigrator() {}

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncer threads involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

void BackendMigrator::MigrateTypes(ModelTypeSet types) {
  const ModelTypeSet old_to_migrate = to_migrate_;
  to_migrate_.PutAll(types);
  SDVLOG(1) << "MigrateTypes called with " << ModelTypeSetToString(types)
            << ", old_to_migrate = " << ModelTypeSetToString(old_to_migrate)
            << ", to_migrate_ = " << ModelTypeSetToString(to_migrate_);
  if (old_to_migrate == to_migrate_) {
    SDVLOG(1) << "MigrateTypes called with no new types; ignoring";
    return;
  }

  if (state_ == IDLE)
    ChangeState(WAITING_TO_START);

  if (state_ == WAITING_TO_START) {
    if (!TryStart())
      SDVLOG(1) << "Manager not configured; waiting";
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
  for (auto& observer : migration_observers_)
    observer.OnMigrationStateChange();
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
            << ModelTypeSetToString(to_migrate_);

  manager_->PurgeForMigration(to_migrate_);
}

void BackendMigrator::OnConfigureDone(
    const DataTypeManager::ConfigureResult& result) {
  if (state_ == IDLE)
    return;

  // |manager_|'s methods aren't re-entrant, and we're notified from
  // them, so post a task to avoid problems.
  SDVLOG(1) << "Posting OnConfigureDoneImpl";
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BackendMigrator::OnConfigureDoneImpl,
                                weak_ptr_factory_.GetWeakPtr(), result));
}

namespace {

ModelTypeSet GetUnsyncedDataTypes(UserShare* user_share) {
  ReadTransaction trans(FROM_HERE, user_share);
  ModelTypeSet unsynced_data_types;
  for (int i = FIRST_REAL_MODEL_TYPE; i < ModelType::NUM_ENTRIES; ++i) {
    ModelType type = ModelTypeFromInt(i);
    sync_pb::DataTypeProgressMarker progress_marker;
    trans.GetDirectory()->GetDownloadProgress(type, &progress_marker);
    if (progress_marker.token().empty()) {
      unsynced_data_types.Put(type);
    }
  }
  return unsynced_data_types;
}

}  // namespace

void BackendMigrator::OnConfigureDoneImpl(
    const DataTypeManager::ConfigureResult& result) {
  SDVLOG(1) << "OnConfigureDone with requested types "
            << ModelTypeSetToString(result.requested_types) << ", status "
            << result.status
            << ", and to_migrate_ = " << ModelTypeSetToString(to_migrate_);
  if (state_ == WAITING_TO_START) {
    if (!TryStart())
      SDVLOG(1) << "Manager still not configured; still waiting";
    return;
  }

  DCHECK_GT(state_, WAITING_TO_START);

  const ModelTypeSet intersection =
      Intersection(result.requested_types, to_migrate_);
  // This intersection check is to determine if our disable request
  // was interrupted by a user changing preferred types.
  if (state_ == DISABLING_TYPES && !intersection.Empty()) {
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
    const ModelTypeSet unsynced_types = GetUnsyncedDataTypes(user_share_);
    if (!unsynced_types.HasAll(to_migrate_)) {
      SLOG(WARNING) << "Set of unsynced types: "
                    << ModelTypeSetToString(unsynced_types)
                    << " does not contain types to migrate: "
                    << ModelTypeSetToString(to_migrate_)
                    << "; not re-enabling yet";
      return;
    }

    ChangeState(REENABLING_TYPES);
    SDVLOG(1) << "BackendMigrator triggering reconfiguration";
    reconfigure_callback_.Run();
  } else if (state_ == REENABLING_TYPES) {
    // We're done!
    ChangeState(IDLE);

    SDVLOG(1) << "BackendMigrator: Migration complete for: "
              << ModelTypeSetToString(to_migrate_);
    to_migrate_.Clear();
    migration_done_callback_.Run();
  }
}

BackendMigrator::State BackendMigrator::state() const {
  return state_;
}

ModelTypeSet BackendMigrator::GetPendingMigrationTypesForTest() const {
  return to_migrate_;
}

#undef SDVLOG

#undef SLOG

}  // namespace syncer
