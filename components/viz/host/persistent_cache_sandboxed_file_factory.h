// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_PERSISTENT_CACHE_SANDBOXED_FILE_FACTORY_H_
#define COMPONENTS_VIZ_HOST_PERSISTENT_CACHE_SANDBOXED_FILE_FACTORY_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/transaction_error.h"
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
  virtual base::expected<persistent_cache::PendingBackend,
                         persistent_cache::TransactionError>
  CreateFiles(const CacheIdString& cache_id, const std::string& product);

  using CreateFilesCallback =
      base::OnceCallback<void(std::optional<persistent_cache::PendingBackend>)>;

  struct VIZ_HOST_EXPORT CreateFilesAsyncOpts {
    // The number of retries to attempt on transient failures before giving up.
    int retries = 0;
    // The delay between retry attempts.
    base::TimeDelta retry_delay = base::TimeDelta();
  };
  // Similar to CreateFiles but will do asynchronously using
  // background_task_runner_. The `callback` will be triggered on the current
  // thread's task runner once the database is created.
  void CreateFilesAsync(const CacheIdString& cache_id,
                        const std::string& product,
                        CreateFilesCallback callback);

  // Overload of CreateFilesAsync that accepts extra options for retries.
  void CreateFilesAsync(const CacheIdString& cache_id,
                        const std::string& product,
                        CreateFilesCallback callback,
                        CreateFilesAsyncOpts extra_opts);

  // Records the on-disk size of the cache files for `cache_id` to UMA from a
  // BEST_EFFORT task, at most once per `cache_id` per session. Only caches
  // whose database already existed when CreateFilesAsync() first opened it for
  // `cache_id` are recorded. Call this once the cache files are known to be
  // used. Does nothing if gpu::kRecordShaderCacheDiskSize is false.
  void RecordDiskSizeAsync(const CacheIdString& cache_id);

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

  // Destroy `backend` asynchronously using background_task_runner_.
  void DeletePendingBackendAsync(persistent_cache::PendingBackend backend);

 protected:
  // Make ctor protected so that the tests can derive and test it directly.
  PersistentCacheSandboxedFileFactory(
      const base::FilePath& cache_root_dir,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  virtual ~PersistentCacheSandboxedFileFactory();

 private:
  friend class base::RefCountedThreadSafe<PersistentCacheSandboxedFileFactory>;

  void CreateFilesAsyncAttempt(const CacheIdString& cache_id,
                               const std::string& product,
                               CreateFilesCallback callback,
                               CreateFilesAsyncOpts extra_opts);

  // The first time this is called for `cache_id`, remembers the cache
  // directory for `cache_id` and `product` if `backend` opened an existing
  // database, so that its size can be recorded later. `backend` must have just
  // been returned by CreateFiles(). Must be called on background_task_runner_.
  void InitializeDiskSizeRecordingState(
      const CacheIdString& cache_id,
      std::string_view product,
      const persistent_cache::PendingBackend& backend);

  // If the database for `cache_id` already existed when CreateFilesAsync()
  // first opened it, posts a BEST_EFFORT task to record its on-disk size, at
  // most once per `cache_id`. Must be called on background_task_runner_.
  void MaybeScheduleDiskSizeRecording(const CacheIdString& cache_id);

  const base::FilePath cache_root_dir_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Bound to background_task_runner_, where the members below are accessed.
  SEQUENCE_CHECKER(background_sequence_checker_);

  // The cache ids that InitializeDiskSizeRecordingState() has been called for.
  base::flat_set<CacheIdString> cache_ids_checked_for_size_recording_
      GUARDED_BY_CONTEXT(background_sequence_checker_);
  // The directories of caches whose database already existed when
  // CreateFilesAsync() first opened it, and whose size hasn't been recorded
  // yet.
  base::flat_map<CacheIdString, base::FilePath>
      cache_dirs_pending_size_recording_
          GUARDED_BY_CONTEXT(background_sequence_checker_);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_PERSISTENT_CACHE_SANDBOXED_FILE_FACTORY_H_
