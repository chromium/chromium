// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/event_storage_migration.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"

namespace feature_engagement {

namespace {
// Number of db to initialize.
const int kNumberOfDBs = 2;
// Type alias for a pair of a key and an event.
using KeyEventPair = std::pair<std::string, Event>;
// Type alias for a list of key and event pairs.
using KeyEventList = std::vector<KeyEventPair>;
}  // namespace

EventStorageMigration::EventStorageMigration(
    raw_ptr<leveldb_proto::ProtoDatabase<Event>> profile_db,
    raw_ptr<leveldb_proto::ProtoDatabase<Event>> device_db)
    : profile_db_(profile_db), device_db_(device_db) {}

EventStorageMigration::~EventStorageMigration() = default;

void EventStorageMigration::Migrate(MigrationCallback callback) {
  // If a request is already in progress, drop the new request.
  if (migration_callback_) {
    return;
  }

  RecordMigrationStatus(EventStorageMigrationStatus::kStarted);

  // Set the callback to be invoked once overall initialization is complete.
  migration_callback_ = std::move(callback);
  // Use a `BarrierClosure` to ensure all async tasks are completed before
  // executing the overall completion callback and returning the data. The
  // BarrierClosure will wait until the `OnInitializationComplete` callback
  // is itself run `kNumberOfDBs` times.
  initialization_complete_barrier_ = base::BarrierClosure(
      kNumberOfDBs,
      base::BindOnce(&EventStorageMigration::OnDBsInitializationCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  // Initialize to true. The overall success is the AND of all individual db
  // initializations. If any of them fail, this will become false.
  initialization_success_ = true;

  profile_db_->Init(
      base::BindOnce(&EventStorageMigration::OnInitializationComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  device_db_->Init(
      base::BindOnce(&EventStorageMigration::OnInitializationComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EventStorageMigration::OnInitializationComplete(
    leveldb_proto::Enums::InitStatus status) {
  bool success = status == leveldb_proto::Enums::InitStatus::kOK;
  // If any of the db fail to initialize, the overall initialization
  // will fail.
  initialization_success_ = initialization_success_ && success;
  // The `BarrierClosure` must be run regardless of the error type to ensure
  // that it is run `kNumberOfDBs` times before the
  // `OnDBsInitializationCompleted` callback can be run.
  initialization_complete_barrier_.Run();
}

void EventStorageMigration::OnDBsInitializationCompleted() {
  if (!initialization_success_) {
    RecordMigrationStatus(EventStorageMigrationStatus::kFailedToInitialize);
    std::move(migration_callback_).Run(false);
    return;
  }

  profile_db_->LoadEntries(
      base::BindOnce(&EventStorageMigration::OnLoadEntriesComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EventStorageMigration::OnLoadEntriesComplete(
    bool success,
    std::unique_ptr<std::vector<Event>> entries) {
  if (!success) {
    RecordMigrationStatus(EventStorageMigrationStatus::kFailedToLoad);
    std::move(migration_callback_).Run(false);
    return;
  }

  // Read profile events into a list.
  std::unique_ptr<KeyEventList> event_list = std::make_unique<KeyEventList>();
  for (const auto& event : *entries) {
    event_list->push_back(KeyEventPair(event.name(), event));
  }

  // Write profile to device db.
  device_db_->UpdateEntries(
      std::move(event_list), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&EventStorageMigration::OnEventWrittenCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EventStorageMigration::OnEventWrittenCompleted(bool success) {
  RecordMigrationStatus(success ? EventStorageMigrationStatus::kCompleted
                                : EventStorageMigrationStatus::kFailedToWrite);

  std::move(migration_callback_).Run(success);
}

// static
void EventStorageMigration::RecordMigrationStatus(
    EventStorageMigrationStatus status) {
  base::UmaHistogramEnumeration("InProductHelp.EventStorageMigration.Status",
                                status);
}

}  // namespace feature_engagement
