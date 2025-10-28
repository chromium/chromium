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

// Abstract interface for DOM storage database implementations. Provides
// key-value storage operations for DOMStorage StorageAreas.
//
// Two instances of this database exists per Profile: one for session storage
// and one for local storage. Records the key-value pairs for all StorageAreas
// along with usage metadata.
//
// Use the `DomStorageDatabaseFactory` to  asynchronously create an instance of
// this type from any sequence. When owning a SequenceBound<DomStorageDatabase>
// as produced by those helpers, all work on the DomStorageDatabase can be
// safely done via `SequenceBound::PostTaskWithThisObject`.
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

  // TODO(crbug.com/377242771): Support both SQLite and LevelDB by adding more
  // shared functions to this interface.

  // For LevelDB only. Rewrites the database on disk to
  // clean up traces of deleted entries.
  //
  // NOTE: If `RewriteDB()` fails, this DomStorageDatabase may no longer
  // be usable; in such cases, all future operations will return an IOError
  // status.
  virtual DbStatus RewriteDB() = 0;

  // Test-only functions.
  virtual bool ShouldFailAllCommits() const = 0;
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

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
