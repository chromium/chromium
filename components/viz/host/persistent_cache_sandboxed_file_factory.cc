// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/persistent_cache_sandboxed_file_factory.h"

#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/base32/base32.h"
#include "components/persistent_cache/backend_storage.h"
#include "components/persistent_cache/backend_type.h"
#include "components/persistent_cache/client.h"
#include "components/persistent_cache/pending_backend.h"

namespace viz {

namespace {

PersistentCacheSandboxedFileFactory* g_instance = nullptr;

std::string GetVersionSuffix(const std::string& product) {
  // The product's version string can be arbitrary long. So use SHA1 to reduce
  // the length to avoid path length limit (260 on Windows and 4096 on Linux).
  // The SHA1 is then encoded using a path-safe base32 (final length = 32
  // characters).
  // TODO(crbug.com/399642827): in future, we should be able to rely on
  // auto-trimming ability of persistent caches, so even if there is a collision
  // in version names, it would still be fine. It's still fine now because the
  // collision probability of SHA1 is 1 in 2^80.
  std::string sha1 = base::SHA1HashString(product);
  return base32::Base32Encode(base::as_byte_span(sha1),
                              base32::Base32EncodePolicy::OMIT_PADDING);
}

// Returns the paths to the directory holding cache files. The format is:
// <cache_dir>/<cache_id>/<version>.
base::FilePath GetPersistentCacheDirectory(
    const base::FilePath& cache_root_dir,
    const base::FilePath::StringType& cache_id,
    const std::string& product) {
  return cache_root_dir.Append(cache_id).AppendASCII(GetVersionSuffix(product));
}

// Deletes all files in the cache directory that are associated with the given
// cache_id but are not the current database or journal file. This is to clean
// up stale cache files from previous runs or different product versions.
void DeleteStaleFiles(const base::FilePath& cache_root_dir,
                      const base::FilePath::StringType& cache_id,
                      const std::string& product) {
  DCHECK(!cache_root_dir.empty());

  const std::string version_suffix = GetVersionSuffix(product);

  bool deleted_stale_cache = false;
  base::FilePath cache_dir = cache_root_dir.Append(cache_id);
  if (base::PathExists(cache_dir)) {
    base::FileEnumerator enumerator(cache_dir, false,
                                    base::FileEnumerator::DIRECTORIES);
    for (base::FilePath name = enumerator.Next(); !name.empty();
         name = enumerator.Next()) {
      if (name.BaseName().MaybeAsASCII() != version_suffix) {
        base::DeletePathRecursively(name);
        deleted_stale_cache = true;
      }
    }
  }

  base::UmaHistogramBoolean("GPU.PersistentCache.StaleCacheDeleted",
                            deleted_stale_cache);
}

bool CreateCacheDirectory(const base::FilePath& cache_dir) {
  if (!base::CreateDirectory(cache_dir)) {
    PLOG(ERROR) << "Failed to create cache directory: " << cache_dir;
    return false;
  }
  return true;
}

}  // namespace

/* static */
void PersistentCacheSandboxedFileFactory::CreateInstance(
    const base::FilePath& cache_root_dir) {
  DCHECK(!g_instance);
  g_instance = new PersistentCacheSandboxedFileFactory(
      cache_root_dir,
      /*task_runner=*/base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
  g_instance->AddRef();
}

/* static */
PersistentCacheSandboxedFileFactory*
PersistentCacheSandboxedFileFactory::GetInstance() {
  return g_instance;
}

/* static */
void PersistentCacheSandboxedFileFactory::SetInstanceForTesting(
    PersistentCacheSandboxedFileFactory* factory) {
  g_instance = factory;
}

PersistentCacheSandboxedFileFactory::PersistentCacheSandboxedFileFactory(
    const base::FilePath& cache_root_dir,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : cache_root_dir_(cache_root_dir),
      background_task_runner_(std::move(background_task_runner)) {
  CHECK(!cache_root_dir_.empty());
  // TODO(crbug.com/399642827): We don't support relative path yet. The flags
  // that are added by AddFlagsForPassingToUntrustedProcess() don't work with
  // relative paths on Windows. See
  // https://source.chromium.org/chromium/chromium/src/+/main:base/files/file_util_win.cc;drc=c99aa55ee638df4d6f0073c5d950acbda6ab4c6d;l=422
  CHECK(cache_root_dir_.IsAbsolute());

  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&CreateCacheDirectory),
                                cache_root_dir_));
}

PersistentCacheSandboxedFileFactory::~PersistentCacheSandboxedFileFactory() =
    default;

base::expected<persistent_cache::PendingBackend,
               persistent_cache::TransactionError>
PersistentCacheSandboxedFileFactory::CreateFiles(const CacheIdString& cache_id,
                                                 const std::string& product) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteStaleFiles, cache_root_dir_, cache_id, product));

  base::FilePath cache_dir =
      GetPersistentCacheDirectory(cache_root_dir_, cache_id, product);
  persistent_cache::BackendStorage cache_storage(
      persistent_cache::Client::kShaderCache,
      persistent_cache::BackendType::kSqlite, cache_dir);
  auto backend = cache_storage.MakePendingBackend(
      base::FilePath(FILE_PATH_LITERAL("cache")), /*single_connection=*/true,
      /*journal_mode_wal=*/true);
  if (!backend.has_value()) {
    PLOG(ERROR) << "Failed to open persistent cache files in directory \""
                << cache_dir
                << "\", error: " << static_cast<int>(backend.error());
  }
  return backend;
}

void PersistentCacheSandboxedFileFactory::CreateFilesAsync(
    const CacheIdString& cache_id,
    const std::string& product,
    CreateFilesCallback callback) {
  CreateFilesAsync(cache_id, product, std::move(callback),
                   CreateFilesAsyncOpts());
}

void PersistentCacheSandboxedFileFactory::CreateFilesAsync(
    const CacheIdString& cache_id,
    const std::string& product,
    CreateFilesCallback callback,
    CreateFilesAsyncOpts extra_opts) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PersistentCacheSandboxedFileFactory::CreateFilesAsyncAttempt, this,
          cache_id, product,
          base::BindPostTaskToCurrentDefault(std::move(callback)),
          std::move(extra_opts)));
}

void PersistentCacheSandboxedFileFactory::CreateFilesAsyncAttempt(
    const CacheIdString& cache_id,
    const std::string& product,
    CreateFilesCallback callback,
    CreateFilesAsyncOpts extra_opts) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  auto backend = CreateFiles(cache_id, product);

  if (backend.has_value()) {
    std::move(callback).Run(*std::move(backend));
  } else if (backend.error() ==
                 persistent_cache::TransactionError::kTransient &&
             extra_opts.retries > 0) {
    const base::TimeDelta delay = extra_opts.retry_delay;
    --extra_opts.retries;

    background_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &PersistentCacheSandboxedFileFactory::CreateFilesAsyncAttempt, this,
            cache_id, product, std::move(callback), std::move(extra_opts)),
        delay);
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

bool PersistentCacheSandboxedFileFactory::ClearFiles(
    const CacheIdString& cache_id,
    const std::string& product) {
  // Delete the whole version directory.
  return base::DeletePathRecursively(
      GetPersistentCacheDirectory(cache_root_dir_, cache_id, product));
}

void PersistentCacheSandboxedFileFactory::ClearFilesAsync(
    const CacheIdString& cache_id,
    const std::string& product,
    ClearFilesCallback callback) {
  // The reply will be posted to the current SequencedTaskRunner.
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PersistentCacheSandboxedFileFactory::ClearFiles, this,
                     cache_id, product),
      std::move(callback));
}

void PersistentCacheSandboxedFileFactory::DeletePendingBackendAsync(
    persistent_cache::PendingBackend backend) {
  // The reply will be posted to the current SequencedTaskRunner.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](persistent_cache::PendingBackend backend) {
                       // Do nothing, simply let the `backend` destructor be
                       // called
                     },
                     std::move(backend)));
}

}  // namespace viz
