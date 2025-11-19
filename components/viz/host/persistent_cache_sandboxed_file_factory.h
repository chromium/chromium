// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_PERSISTENT_CACHE_SANDBOXED_FILE_FACTORY_H_
#define COMPONENTS_VIZ_HOST_PERSISTENT_CACHE_SANDBOXED_FILE_FACTORY_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/viz/host/viz_host_export.h"

namespace viz {

// This class supports opening file handles in a persistent cache directory.
// The handles can be forwarded to the GPU process to load & store blobs.
//
// The persistent cache files are versioned based on the provided product
// version. When a version change happens, the older versioned files will
// be automatically deleted. TODO(crbug.com/399642827): This is a temporary
// solution until PersistentCache supports max size limit and trimming.
class VIZ_HOST_EXPORT PersistentCacheSandboxedFileFactory
    : public base::RefCountedThreadSafe<PersistentCacheSandboxedFileFactory> {
 public:
  using CacheIdString = base::FilePath::StringType;

  static void CreateInstance(const base::FilePath& cache_root_dir);
  static PersistentCacheSandboxedFileFactory* GetInstance();
  static void SetInstanceForTesting(
      PersistentCacheSandboxedFileFactory* factory);

  PersistentCacheSandboxedFileFactory(
      const PersistentCacheSandboxedFileFactory&) = delete;
  PersistentCacheSandboxedFileFactory& operator=(
      const PersistentCacheSandboxedFileFactory&) = delete;

  // Creates the persistent cache database and journal files.
  // `cache_id` is used to uniquely identify the cache type (e.g.,
  // 'dawngraphite'). `product` is used for versioning. Stale files from
  // different versions are automatically deleted.
  std::optional<persistent_cache::PendingBackend> CreateFiles(
      const CacheIdString& cache_id,
      const std::string& product);

  using CreateFilesCallback =
      base::OnceCallback<void(std::optional<persistent_cache::PendingBackend>)>;
  // Similar to CreateFiles but will do asynchronously using
  // background_task_runner_. The `callback` will be triggered on the current
  // thread's task runner once the deletion is completed.
  void CreateFilesAsync(const CacheIdString& cache_id,
                        const std::string& product,
                        CreateFilesCallback callback);

  // Deletes the persistent cache files.
  // `cache_id` is the unique identifier for the cache type.
  // `product` is the browser product string, used to identify the specific
  // version of the cache files to delete.
  bool ClearFiles(const CacheIdString& cache_id, const std::string& product);

  using ClearFilesCallback = base::OnceCallback<void(bool)>;
  // Similar to ClearFiles but will do asynchronously using
  // background_task_runner_. The `callback` will be triggered on the current
  // thread's task runner once the deletion is completed.
  void ClearFilesAsync(const CacheIdString& cache_id,
                       const std::string& product,
                       ClearFilesCallback callback);

 protected:
  // Make ctor protected so that the tests can derive and test it directly.
  PersistentCacheSandboxedFileFactory(
      const base::FilePath& cache_root_dir,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  virtual ~PersistentCacheSandboxedFileFactory();

 private:
  friend class base::RefCountedThreadSafe<PersistentCacheSandboxedFileFactory>;

  const base::FilePath cache_root_dir_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_PERSISTENT_CACHE_SANDBOXED_FILE_FACTORY_H_
