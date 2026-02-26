// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_PENDING_BACKEND_H_
#define COMPONENTS_PERSISTENT_CACHE_PENDING_BACKEND_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "components/sqlite_vfs/pending_file_set.h"

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo

namespace persistent_cache {

namespace mojom {
class PendingReadOnlyBackendDataView;
class PendingReadWriteBackendDataView;
}  // namespace mojom

namespace sqlite {
class BackendStorageDelegate;
}  // namespace sqlite

// The state required to connect to a backend. Instances are created via a
// BackendStorage and are bound by a PersistentCache to establish a connection.
struct COMPONENT_EXPORT(PERSISTENT_CACHE) PendingBackend {
  PendingBackend();
  PendingBackend(PendingBackend&&);
  PendingBackend& operator=(PendingBackend&&);
  ~PendingBackend();

 private:
  friend class SqliteBackendImpl;
  friend class persistent_cache::sqlite::BackendStorageDelegate;
  friend struct mojo::StructTraits<
      persistent_cache::mojom::PendingReadOnlyBackendDataView,
      persistent_cache::PendingBackend>;
  friend struct mojo::StructTraits<
      persistent_cache::mojom::PendingReadWriteBackendDataView,
      persistent_cache::PendingBackend>;
  FRIEND_TEST_ALL_PREFIXES(BackendStorageTest, MakePendingBackendSucceeds);
  FRIEND_TEST_ALL_PREFIXES(PersistentCacheReadOnlyMojomTraitsTest, Do);
  FRIEND_TEST_ALL_PREFIXES(PersistentCacheReadWriteMojomTraitsTest, Do);
  FRIEND_TEST_ALL_PREFIXES(PersistentCacheTest, RecoveryFromTransientError);
  FRIEND_TEST_ALL_PREFIXES(SQLiteBackendImplTest,
                           ReopeningFilesWithSameUserVersionWorks);
  FRIEND_TEST_ALL_PREFIXES(SQLiteBackendImplTest,
                           VersionMismatchLeadsToFailedInitializeWhenReadOnly);
  FRIEND_TEST_ALL_PREFIXES(SQLiteBackendImplTest,
                           VersionMismatchDropsTablesWithReadWriteConnection);

  explicit PendingBackend(sqlite_vfs::PendingFileSet pending_file_set);

  // The data specific to the SQLite backend. If there is ever occasion to have
  // more than one type, use std::variant<> to hold the data for each.
  sqlite_vfs::PendingFileSet pending_file_set;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_PENDING_BACKEND_H_
