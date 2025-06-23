// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_STORAGE_MIGRATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_STORAGE_MIGRATION_H_

#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace feature_engagement {

// A EventStorageMigration provides the ability to migrate the event storage
// from the profile db to the device db.
// TODO(crbug.com/426624087): Remove this and all the calls related to it once
// the migration is completed.
class EventStorageMigration {
 public:
  // Represents the status of the profile to device migration.
  // These enums are persisted as histogram entries, so this enum should be
  // treated as append-only and kept in sync with
  // InProductHelpEventStorageMigrationStatus in enums.xml.
  enum class EventStorageMigrationStatus : int {
    // The migration is not required.
    kNotRequired = 0,
    // The migration has started.
    kStarted = 1,
    // The migration has completed successfully.
    kCompleted = 2,
    // The migration failed to initialize.
    kFailedToInitialize = 3,
    // The migration failed to load.
    kFailedToLoad = 4,
    // The migration failed to write.
    kFailedToWrite = 5,
    // The maximum value of the enum.
    kMaxValue = kFailedToWrite
  };

  explicit EventStorageMigration(
      raw_ptr<leveldb_proto::ProtoDatabase<Event>> profile_db,
      raw_ptr<leveldb_proto::ProtoDatabase<Event>> device_db);

  EventStorageMigration(const EventStorageMigration&) = delete;
  EventStorageMigration& operator=(const EventStorageMigration&) = delete;

  ~EventStorageMigration();

  static void RecordMigrationStatus(EventStorageMigrationStatus status);

  // Callback for when migration has finished. The |success|
  // argument denotes whether the migration was successful.
  using MigrationCallback = base::OnceCallback<void(bool success)>;

  // Migrate the event storage from the profile db to the device db.
  void Migrate(MigrationCallback callback);

 private:
  // Callback for when an underlying db has been initialized.
  void OnInitializationComplete(leveldb_proto::Enums::InitStatus status);

  // Called when both underlying db have finished their initialization.
  void OnDBsInitializationCompleted();

  // Called when the profile db has finished loading.
  void OnLoadEntriesComplete(bool success,
                             std::unique_ptr<std::vector<Event>> entries);

  // Called when the device db has finished writing.
  void OnEventWrittenCompleted(bool success);

  // Barrier closure that is run when both db have initialized.
  base::RepeatingClosure initialization_complete_barrier_;

  // Callback to be invoked once overall migration is complete.
  MigrationCallback migration_callback_;

  // Tracks the overall success of the initialization process. True if both
  // underlying db initialize successfully.
  bool initialization_success_ = false;

  // The profile db.
  raw_ptr<leveldb_proto::ProtoDatabase<Event>> profile_db_;

  // The device db.
  raw_ptr<leveldb_proto::ProtoDatabase<Event>> device_db_;

  base::WeakPtrFactory<EventStorageMigration> weak_ptr_factory_{this};
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_EVENT_STORAGE_MIGRATION_H_
