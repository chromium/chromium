// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_

#include "base/component_export.h"
#include "base/features.h"

namespace storage {

// Enable to use a SQLite database with local storage and session storage
// instead of LevelDB. See "crbug.com/377242771: Migrate DOMStorage to use
// SQLite" for more details. When enabled, SQLite is used for both in-memory
// and on-disk storage.
COMPONENT_EXPORT(STORAGE_FEATURES) BASE_DECLARE_FEATURE(kDomStorageSqlite);

// Enable to use a SQLite database for in-memory local storage and session
// storage only. When enabled, the SQLite backend will be used for in-memory
// scenarios (e.g., Incognito mode) while on-disk storage continues to use
// LevelDB. If `kDomStorageSqlite` is enabled, SQLite is used for all scenarios
// regardless of this feature's state.
COMPONENT_EXPORT(STORAGE_FEATURES)
BASE_DECLARE_FEATURE(kDomStorageSqliteInMemory);

// Enable to migrate on-disk LevelDB databases to SQLite after they become idle.
// Also, uses SQLite for new databases.
COMPONENT_EXPORT(STORAGE_FEATURES)
BASE_DECLARE_FEATURE(kDomStorageSqliteMigration);

// How long an on-disk LevelDB must stay idle before it is migrated to SQLite.
// Exposed as a feature param so field trials can tune it and tests can
// shorten it.
COMPONENT_EXPORT(STORAGE_FEATURES)
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kDomStorageSqliteMigrationInactivityTimeoutParam);

inline constexpr base::TimeDelta kDomStorageSqliteMigrationInactivityTimeout =
    base::Seconds(45);

// Used as a feature param by `kDomStorageSqliteNewDatabases`. Adding, removing
// and reordering values is fine; just make sure to update
// `kDomStorageSqliteNewDatabasesStages` when adding new values.
enum class DomStorageSqliteRolloutStage {
  // Use LevelDB exclusively. All on-disk stores emit metrics to "OnDisk".
  kUseLevelDbOnly,
  // Functionally the same as `kUseLevelDbOnly`. On-disk stores created during
  // this stage emit metrics to "OnDiskExperimental"; previously existing stores
  // emit to "OnDisk".
  kUseLevelDbAsControl,
  // Use SQLite for new databases only. On-disk SQLite stores emit metrics to
  // "OnDiskExperimental"; on-disk LevelDB stores emit to "OnDisk".
  kUseSqliteForNewDatabases,
  // Use SQLite exclusively. All on-disk stores emit metrics to "OnDisk".
  kUseSqliteOnly,
};

// Enable to control the on-disk rollout of the SQLite backend for DomStorage
// on new databases. The rollout stage is controlled by the
// "DomStorageSqliteNewDatabasesStage" param.
COMPONENT_EXPORT(STORAGE_FEATURES)
BASE_DECLARE_FEATURE(kDomStorageSqliteNewDatabases);
COMPONENT_EXPORT(STORAGE_FEATURES)
BASE_DECLARE_FEATURE_PARAM(DomStorageSqliteRolloutStage,
                           kDomStorageSqliteNewDatabasesStage);

// Returns the SQLite rollout stage for a DomStorage database, taking into
// account all relevant feature flags. For in-memory databases, the result
// is always `kUseSqliteOnly` or `kUseLevelDbOnly` because the experimental
// on-disk stages do not apply.
COMPONENT_EXPORT(STORAGE_FEATURES)
DomStorageSqliteRolloutStage GetSqliteRolloutStage(bool in_memory);

// Returns true if the rollout stage is an experimental stage that requires
// checking disk state to determine the database path and histogram suffix.
COMPONENT_EXPORT(STORAGE_FEATURES)
bool IsExperimentalRolloutStage(DomStorageSqliteRolloutStage stage);

// Returns true if the given stage and disk state indicate SQLite should be
// used. `leveldb_exists` should be true if a non-empty LevelDB directory
// exists on disk.
COMPONENT_EXPORT(STORAGE_FEATURES)
bool ShouldUseSqlite(DomStorageSqliteRolloutStage stage, bool leveldb_exists);

// Returns true if the experimental tag file should be written after creating
// a new LevelDB database.
COMPONENT_EXPORT(STORAGE_FEATURES)
bool ShouldWriteExpTag(DomStorageSqliteRolloutStage stage, bool leveldb_exists);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_
