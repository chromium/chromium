// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_BACKEND_STORAGE_H_
#define COMPONENTS_PERSISTENT_CACHE_BACKEND_STORAGE_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace persistent_cache {

class Backend;
enum class BackendType;
class PersistentCache;
struct PendingBackend;

// Manages the storage of files for backends within a directory. Only one
// instance per process is permitted to operate on a given directory at a time.
class COMPONENT_EXPORT(PERSISTENT_CACHE) BackendStorage {
 public:
  // A delegate interface to be implemented by each concrete type of backend.
  // The delegate is responsible for managing backends in a directory identified
  // by their base names.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns a new pending read-write backend named `base_name` within
    // `directory`. If `single_connection` is true, the returned instance may be
    // used to open only one `PersistentCache` -- connections to that cache
    // cannot be shared. If `journal_mode_wal` (which only applies to the SQLite
    // backend) is true, the database will use write-ahead log journaling.
    // Returns no value in case of error (e.g., if the backend's files could not
    // be opened or created).
    virtual std::optional<PendingBackend> MakePendingBackend(
        const base::FilePath& directory,
        const base::FilePath& base_name,
        bool single_connection,
        bool journal_mode_wal) = 0;

    // Returns a new read-write backend named `base_name` within `directory`. If
    // `single_connection` is true, the returned backend may be used by only one
    // `PersistentCache` instance -- connections to it cannot be shared for use
    // by other instances. If `journal_mode_wal` (which only applies to the
    // SQLite backend) is true, the database will use write-ahead log
    // journaling. Returns null in case of error (e.g., if the backend's files
    // could not be opened or created, or if the backend's storage is corrupt).
    virtual std::unique_ptr<Backend> MakeBackend(
        const base::FilePath& directory,
        const base::FilePath& base_name,
        bool single_connection,
        bool journal_mode_wal) = 0;

    // Returns a pending backend for a read-only connection to the backend named
    // `base_name` within `directory`. This allows another party to bind to an
    // existing backend.
    virtual std::optional<PendingBackend> ShareReadOnlyConnection(
        const base::FilePath& directory,
        const base::FilePath& base_name,
        const Backend& backend) = 0;

    // Returns a pending backend for a read-write connection to the backend
    // named `base_name` within `directory`. This allows another party to bind
    // to an existing backend.
    virtual std::optional<PendingBackend> ShareReadWriteConnection(
        const base::FilePath& directory,
        const base::FilePath& base_name,
        const Backend& backend) = 0;

    // Returns the basename of `file` if it names a file managed by the backend,
    // or an empty path otherwise.
    virtual base::FilePath GetBaseName(const base::FilePath& file) = 0;

    // Deletes all files corresponding to the backend named `base_name` in
    // `directory`. Returns the total size, in bytes, of all files deleted.
    virtual int64_t DeleteFiles(const base::FilePath& directory,
                                const base::FilePath& base_name) = 0;

   protected:
    Delegate() = default;
  };

  // Constructs an instance that will use the given backend type for file
  // management within `directory`. Creates `directory` if it does not already
  // exist.
  BackendStorage(BackendType backend_type, base::FilePath directory);

  // Constructs an instance that will use `delegate` for file management within
  // `directory`. Creates `directory` if it does not already exist.
  BackendStorage(std::unique_ptr<Delegate> delegate, base::FilePath directory);
  BackendStorage(const BackendStorage&) = delete;
  BackendStorage& operator=(const BackendStorage&) = delete;
  ~BackendStorage();

  // Returns the directory managed by the instance.
  const base::FilePath& directory() const { return directory_; }

  // Returns a new pending read-write backend named `base_name` within the
  // instance's directory. If `single_connection` is true, the returned instance
  // may be used to open only one `PersistentCache` -- connections to that cache
  // cannot be shared. If `journal_mode_wal` (which only applies to the SQLite
  // backend) is true, the database will use write-ahead log journaling. Returns
  // no value in case of error (e.g., if the backend's files could not be opened
  // or created).
  std::optional<PendingBackend> MakePendingBackend(
      const base::FilePath& base_name,
      bool single_connection,
      bool journal_mode_wal);

  // Returns a new read-write backend named `base_name` within the instance's
  // directory. If `single_connection` is true, the returned backend may be used
  // by only one `PersistentCache` instance -- connections to it cannot be
  // shared for use by other instances. If `journal_mode_wal` (which only
  // applies to the SQLite backend) is true, the database will use write-ahead
  // log journaling. Returns null in case of error (e.g., if the backend's files
  // could not be opened or created, or the backend's storage is corrupt).
  std::unique_ptr<Backend> MakeBackend(const base::FilePath& base_name,
                                       bool single_connection,
                                       bool journal_mode_wal);

  // Returns a pending backend for a read-only connection to the backend named
  // `base_name` within the instance's directory. This allows another party to
  // bind to an existing backend.
  std::optional<PendingBackend> ShareReadOnlyConnection(
      const base::FilePath& base_name,
      const PersistentCache& cache);

  // Returns a pending backend for a read-write connection to the backend named
  // `base_name` within the instance's directory. This allows another party to
  // bind to an existing backend.
  std::optional<PendingBackend> ShareReadWriteConnection(
      const base::FilePath& base_name,
      const PersistentCache& cache);

  // Deletes all files in the instance's directory. Any outstanding backend
  // instances will continue to operate on the deleted files, and no new
  // backends using them should be created. An attempt to do so may fail (and
  // likely will on Windows). The caller should ensure that all outstanding
  // backends are destroyed before creating new ones in the managed directory.
  void DeleteAllFiles();

  // Delete files associated with `base_name` within the instance's directory.
  // The conditions apply as in `DeleteAllFiles`.
  void DeleteFiles(const base::FilePath& base_name);

  struct FootprintReductionResult {
    int64_t current_footprint;
    int64_t number_of_bytes_deleted;
  };

  // Deletes backend files from oldest-to-newest to bring the total disk usage
  // within the instance's directory down to `target_footprint`. Returns the
  // current footprint and the number of bytes deleted in the operation, if any.
  FootprintReductionResult BringDownTotalFootprintOfFiles(
      int64_t target_footprint);

 private:
  // The delegate used to create/operate on backends.
  const std::unique_ptr<Delegate> delegate_;

  // The directory in which backends reside.
  const base::FilePath directory_;

  // True if the instance has succeeded in creating its directory.
  bool is_valid_ = false;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_BACKEND_STORAGE_H_
