// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_BACKEND_STORAGE_H_
#define COMPONENTS_PERSISTENT_CACHE_BACKEND_STORAGE_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace persistent_cache {

class Backend;

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

    // Returns a backend named `base_name` supporting read/write access backed
    // by one or more files in `directory`. Returns null in case of any error.
    virtual std::unique_ptr<Backend> MakeBackend(
        const base::FilePath& directory,
        const base::FilePath& base_name) = 0;

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

  // Constructs an instance that will use the default backend for file
  // management within `directory`. Creates `directory` if it does not already
  // exist.
  explicit BackendStorage(base::FilePath directory);

  // Constructs an instance that will use `delegate` for file management within
  // `directory`. Creates `directory` if it does not already exist.
  BackendStorage(std::unique_ptr<Delegate> delegate, base::FilePath directory);
  BackendStorage(const BackendStorage&) = delete;
  BackendStorage& operator=(const BackendStorage&) = delete;
  ~BackendStorage();

  // Returns the directory managed by the instance.
  const base::FilePath& directory() const { return directory_; }

  // Returns a new backend named `base_name` within the instance's directory.
  std::unique_ptr<Backend> MakeBackend(const base::FilePath& base_name);

  // Deletes all files in the instance's directory. Any outstanding backend
  // instances will continue to operate on the deleted files, and no new
  // backends using them should be created. An attempt to do so may fail (and
  // likely will on Windows). The caller should ensure that all outstanding
  // backends are destroyed before creating new ones in the managed directory.
  void DeleteAllFiles();

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
