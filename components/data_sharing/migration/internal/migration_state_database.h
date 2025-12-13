// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATION_STATE_DATABASE_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATION_STATE_DATABASE_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "components/data_sharing/migration/internal/protocol/migration_state.pb.h"
#include "components/data_sharing/migration/public/context_id.h"

namespace data_sharing {

// A SQLite database for storing data_sharing migration state. This database
// stores a proto for each sync entity, keyed by the context id.
class MigrationStateDatabase {
 public:
  // Callback for when the database has been initialized. The boolean argument
  // indicates success or failure.
  using InitCallback = base::OnceCallback<void(bool)>;

  virtual ~MigrationStateDatabase() = default;

  // Initializes the database at the given `db_path`. Must be called before any
  // other methods.
  virtual void Init(InitCallback callback) = 0;

  // Synchronously retrieves the migration state for a given `context_id`.
  // Returns std::nullopt if the entry is not found. Must be called after Init()
  // has completed successfully.
  virtual std::optional<data_sharing_pb::MigrationState> GetMigrationState(
      const ContextId& context_id) const = 0;

  // Synchronously adds or updates the migration state for a given
  // `context_id`. The database will write to disk asynchronously after.
  virtual void UpdateMigrationState(
      const ContextId& context_id,
      const data_sharing_pb::MigrationState& state) = 0;

  // Synchronously deletes the migration state for a given `context_id`.
  virtual void DeleteMigrationState(const ContextId& context_id) = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATION_STATE_DATABASE_H_
