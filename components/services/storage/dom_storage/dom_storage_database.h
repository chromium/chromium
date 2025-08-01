// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "storage/common/database/db_status.h"

namespace base {
class FilePath;
namespace trace_event {
class MemoryAllocatorDumpGuid;
}  // namespace trace_event
}  // namespace base

namespace storage {

class DomStorageBatchOperation;

// Abstract interface for DOM storage database implementations. Provides
// key-value storage operations for DOMStorage StorageAreas.
//
// An instance of this database exists per Profile. The `storage_key` prefix is
// used to organize key-value pairs for a StorageArea. It enables efficient
// prefix-based operations to manipulate data for entire storage areas.

// Use the static `OpenInMemory()` or `OpenDirectory()` helpers to
// asynchronously create an instance of this type from any sequence.
// When owning a SequenceBound<DomStorageDatabase> as produced by
// those helpers, all work on the DomStorageDatabase can be safely done via
// `SequenceBound::PostTaskWithThisObject`.
class DomStorageDatabase {
 public:
  using Key = std::vector<uint8_t>;
  using KeyView = base::span<const uint8_t>;
  using Value = std::vector<uint8_t>;
  using ValueView = base::span<const uint8_t>;

  struct KeyValuePair {
    KeyValuePair();
    KeyValuePair(KeyValuePair&&);
    KeyValuePair(const KeyValuePair&);
    KeyValuePair(Key key, Value value);
    ~KeyValuePair();
    KeyValuePair& operator=(KeyValuePair&&);
    KeyValuePair& operator=(const KeyValuePair&);

    bool operator==(const KeyValuePair& rhs) const;

    Key key;
    Value value;
  };

  virtual ~DomStorageDatabase() = default;

  // Retrieves the value for |key| in the database.
  virtual DbStatus Get(KeyView key, Value* out_value) const = 0;

  // Sets the database entry for |key| to |value|.
  virtual DbStatus Put(KeyView key, ValueView value) const = 0;

  // Gets all database entries whose key starts with |prefix|.
  virtual DbStatus GetPrefixed(KeyView prefix,
                               std::vector<KeyValuePair>* entries) const = 0;

  // Rewrites the database on disk to clean up traces of deleted entries.
  //
  // NOTE: If |RewriteDB()| fails, this DomStorageDatabase may no longer
  // be usable; in such cases, all future operations will return an IOError
  // status.
  virtual DbStatus RewriteDB() = 0;

  // Returns a database implementation appropriate batch operation for
  // atomically applying multiple database updates. The returned object is not
  // thread safe. It should be accessed from the same sequence it was created
  // on. The returned object must not outlive the DomStorageDatabase instance
  // it was created from.
  virtual std::unique_ptr<DomStorageBatchOperation> CreateBatchOperation() = 0;
  virtual bool ShouldFailAllCommits() const = 0;

  // Test only methods.
  virtual void MakeAllCommitsFailForTesting() = 0;
  virtual void SetDestructionCallbackForTesting(base::OnceClosure callback) = 0;
};

class DomStorageDatabaseFactory {
 public:
  // Callback invoked asynchronously with the result of both `OpenDirectory()`
  // and `OpenInMemory()` defined below. Includes both the status and the
  // (possibly null, on failure) sequence-bound DomStorageDatabase instance.
  using OpenCallback =
      base::OnceCallback<void(base::SequenceBound<DomStorageDatabase> database,
                              DbStatus status)>;
  // Creates a DomStorageDatabase instance for a persistent database
  // within a filesystem directory given by `directory`, which must be an
  // absolute path. The database may or may not already exist at this path, and
  // will be created if not.
  //
  // The instance will be bound to and perform all operations on `task_runner`,
  // which must support blocking operations. `callback` is called on the calling
  // sequence once the operation completes.
  static void OpenDirectory(
      const base::FilePath& directory,
      const std::string& name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      OpenCallback callback);

  // Creates a DomStorageDatabase instance for a new in-memory database.
  //
  // The instance will be bound to and perform all operations on `task_runner`,
  // which must support blocking operations. `callback` is called on the calling
  // sequence once the operation completes.
  static void OpenInMemory(
      const std::string& name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      OpenCallback callback);

  // Destroys the persistent database named `name` within the filesystem
  // directory identified by the absolute path in `directory`.
  //
  // All work is done on `task_runner`, which must support blocking operations,
  // and upon completion `callback` is called on the calling sequence.
  static void Destroy(
      const base::FilePath& directory,
      const std::string& name,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      base::OnceCallback<void(DbStatus)> callback);
};

// Abstraction for batched operations on a DomStorageDatabase.
// This class encapsulates a series of database operations that should be
// performed atomically.
class DomStorageBatchOperation {
 public:
  using Key = DomStorageDatabase::Key;
  using KeyView = DomStorageDatabase::KeyView;
  using Value = DomStorageDatabase::Value;
  using ValueView = DomStorageDatabase::ValueView;

  virtual ~DomStorageBatchOperation() = default;

  // Store the mapping "key->value" in the database.
  virtual void Put(KeyView key, ValueView value) = 0;

  // Delete the entry for "key" if it exists.
  virtual void Delete(KeyView key) = 0;

  // Adds operations to |batch| which will delete all database entries whose key
  // starts with |prefix| when committed.
  virtual DbStatus DeletePrefixed(KeyView prefix) = 0;

  // Adds operations to |batch| which when committed will copy all database
  // entries whose key starts with |prefix| over to new entries with |prefix|
  // replaced by |new_prefix| in each new key.
  virtual DbStatus CopyPrefixed(KeyView prefix, KeyView new_prefix) = 0;

  // Commits operations in |batch| to the database.
  virtual DbStatus Commit() = 0;

  // The size of the database changes caused by this batch operation. This
  // number is tied to implementation details and should only be used for
  // metrics.
  virtual size_t ApproximateSizeForMetrics() const = 0;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
