// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/types/pass_key.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace storage {

// Wraps its own leveldb::DB instance on behalf of the DOM Storage backend
// implementation. This object is not sequence-safe and must be instantiated on
// a sequence which allows use of blocking file operations.
//
// Use the static |OpenInMemory()| or |OpenDirectory()| helpers to
// asynchronously create an instance of this type from any sequence.
// When owning a SequenceBound<DomStorageDatabase> as produced by these helpers,
// all work on the DomStorageDatabase can be safely done via
// |SequenceBound::PostTaskWithThisObject|.
class DomStorageDatabase : private base::trace_event::MemoryDumpProvider {
 private:
  using PassKey = base::PassKey<DomStorageDatabase>;

 public:
  using Key = std::vector<uint8_t>;
  using KeyView = base::span<const uint8_t>;
  using Value = std::vector<uint8_t>;
  using ValueView = base::span<const uint8_t>;
  using Status = leveldb::Status;

  // Callback used for basic async operations on this class.
  using StatusCallback = base::OnceCallback<void(Status)>;

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

  DomStorageDatabase(const DomStorageDatabase&) = delete;
  DomStorageDatabase& operator=(const DomStorageDatabase&) = delete;

  ~DomStorageDatabase() override;

  // Callback invoked asynchronously with the result of both |OpenDirectory()|
  // and |OpenInMemory()| defined below. Includes both the status and the
  // (possibly null, on failure) sequence-bound DomStorageDatabase instance.
  using OpenCallback =
      base::OnceCallback<void(base::SequenceBound<DomStorageDatabase> database,
                              leveldb::Status status)>;

  // Creates a DomStorageDatabase instance for a persistent database within a
  // filesystem directory given by |directory|, which must be an absolute path.
  // The database may or may not already exist at this path, and will be created
  // if not.
  //
  // The instance will be bound to and perform all operations on |task_runner|,
  // which must support blocking operations. |callback| is called on the calling
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
  // The instance will be bound to and perform all operations on |task_runner|,
  // which must support blocking operations. |callback| is called on the calling
  // sequence once the operation completes.
  static void OpenInMemory(
      const std::string& name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      OpenCallback callback);

  // Destroys the persistent database named |name| within the filesystem
  // directory identified by the absolute path in |directory|.
  //
  // All work is done on |task_runner|, which must support blocking operations,
  // and upon completion |callback| is called on the calling sequence.
  static void Destroy(
      const base::FilePath& directory,
      const std::string& name,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      StatusCallback callback);

  // Retrieves the value for |key| in the database.
  Status Get(KeyView key, Value* out_value) const;

  // Sets the database entry for |key| to |value|.
  Status Put(KeyView key, ValueView value) const;

  // Gets all database entries whose key starts with |prefix|.
  Status GetPrefixed(KeyView prefix, std::vector<KeyValuePair>* entries) const;

  // Adds operations to |batch| which will delete all database entries whose key
  // starts with |prefix| when committed.
  Status DeletePrefixed(KeyView prefix, leveldb::WriteBatch* batch) const;

  // Adds operations to |batch| which when committed will copy all database
  // entries whose key starts with |prefix| over to new entries with |prefix|
  // replaced by |new_prefix| in each new key.
  Status CopyPrefixed(KeyView prefix,
                      KeyView new_prefix,
                      leveldb::WriteBatch* batch) const;

  // Commits operations in |batch| to the database.
  Status Commit(leveldb::WriteBatch* batch) const;

  // Rewrites the database on disk to clean up traces of deleted entries.
  //
  // NOTE: If |RewriteDB()| fails, this DomStorageDatabase may no longer be
  // usable; in such cases, all future operations will return an IOError status.
  Status RewriteDB();

  void SetDestructionCallbackForTesting(base::OnceClosure callback) {
    destruction_callback_ = std::move(callback);
  }

  void MakeAllCommitsFailForTesting() { fail_commits_for_testing_ = true; }

  // Use the static factory functions above to construct this class. These
  // constructors are only public for the sake of `base::SequenceBound`.
  DomStorageDatabase(
      PassKey,
      const base::FilePath& directory,
      const std::string& name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      StatusCallback callback);
  DomStorageDatabase(
      PassKey,
      const std::string& tracking_name,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      StatusCallback callback);

 private:
  // Initializes a new DomStorageDatabase, creating or opening persistent
  // on-filesystem database as specified. Asynchronously invokes `callback` on
  // `callback_task_runner` when done.
  //
  // This must be called on a sequence that allows blocking operations.
  void Init(scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
            StatusCallback callback);

  template <typename... Args>
  static void CreateSequenceBoundDomStorageDatabase(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      DomStorageDatabase::OpenCallback callback,
      Args&&... args);

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  const std::string name_;
  const std::unique_ptr<leveldb::Env> env_;
  leveldb_env::Options options_;
  const std::optional<base::trace_event::MemoryAllocatorDumpGuid>
      memory_dump_id_;
  std::unique_ptr<leveldb::DB> db_;

  // Causes all calls to |Commit()| to fail with an IOError for simulated
  // disk failures in testing.
  bool fail_commits_for_testing_ = false;

  // Callback to run on destruction in tests.
  base::OnceClosure destruction_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_DATABASE_H_
