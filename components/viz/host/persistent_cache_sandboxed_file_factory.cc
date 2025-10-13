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
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/base32/base32.h"
#include "components/persistent_cache/sqlite/vfs/sandboxed_file.h"

namespace viz {

namespace {

PersistentCacheSandboxedFileFactory* g_instance = nullptr;

struct PersistentCacheFilePaths {
  base::FilePath db_path;
  base::FilePath journal_path;
};

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

// Returns the paths to the cache database and journal files. The format is:
// <cache_dir>/<cache_id>/<version>/cache.db
// <cache_dir>/<cache_id>/<version>/cache.journal
PersistentCacheFilePaths GetPersistentCacheFilePaths(
    const base::FilePath& cache_root_dir,
    const base::FilePath::StringType& cache_id,
    const std::string& product) {
  base::FilePath version_dir =
      cache_root_dir.Append(cache_id).AppendASCII(GetVersionSuffix(product));

  return {version_dir.AppendASCII("cache.db"),
          version_dir.AppendASCII("cache.journal")};
}

// Deletes all files in the cache directory that are associated with the given
// cache_id but are not the current database or journal file. This is to clean
// up stale cache files from previous runs or different product versions.
void DeleteStaleFiles(const base::FilePath& cache_root_dir,
                      const base::FilePath::StringType& cache_id,
                      const std::string& product) {
  DCHECK(!cache_root_dir.empty());

  const std::string version_suffix = GetVersionSuffix(product);

  base::FilePath cache_dir = cache_root_dir.Append(cache_id);
  if (!base::PathExists(cache_dir)) {
    return;
  }

  base::FileEnumerator enumerator(cache_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    if (name.BaseName().MaybeAsASCII() != version_suffix) {
      base::DeletePathRecursively(name);
    }
  }
}

bool CreateCacheDirectory(const base::FilePath& cache_dir) {
  if (!base::CreateDirectory(cache_dir)) {
    LOG(ERROR) << "Failed to create cache directory: " << cache_dir;
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

  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& dir) { CreateCacheDirectory(dir); },
          cache_root_dir_));
}

PersistentCacheSandboxedFileFactory::~PersistentCacheSandboxedFileFactory() =
    default;

std::optional<persistent_cache::BackendParams>
PersistentCacheSandboxedFileFactory::CreateFiles(const CacheIdString& cache_id,
                                                 const std::string& product) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteStaleFiles, cache_root_dir_, cache_id, product));

  DCHECK(!cache_root_dir_.empty());

  auto paths = GetPersistentCacheFilePaths(cache_root_dir_, cache_id, product);
  DCHECK_EQ(paths.db_path.DirName(), paths.journal_path.DirName());

  if (!CreateCacheDirectory(paths.db_path.DirName())) {
    return std::nullopt;
  }

  auto open_and_check_file = [](const base::FilePath& path) {
    const auto flags = base::File::AddFlagsForPassingToUntrustedProcess(
        base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
        base::File::FLAG_WRITE);
    base::File file(path, flags);
    if (!file.IsValid()) {
      LOG(ERROR) << "Failed to open persistent cache file: " << path
                 << " error: "
                 << base::File::ErrorToString(file.error_details());
    }
    return file;
  };

  persistent_cache::BackendParams params;

  params.type = persistent_cache::BackendType::kSqlite;
  params.db_file = open_and_check_file(paths.db_path);
  if (!params.db_file.IsValid()) {
    return std::nullopt;
  }
  params.db_file_is_writable = true;

  params.journal_file = open_and_check_file(paths.journal_path);
  if (!params.journal_file.IsValid()) {
    return std::nullopt;
  }
  params.journal_file_is_writable = true;

  params.shared_lock = base::UnsafeSharedMemoryRegion::Create(
      sizeof(persistent_cache::LockState));
  if (!params.shared_lock.IsValid()) {
    LOG(ERROR) << "Failed to create shared lock";
    return std::nullopt;
  }

  return params;
}

void PersistentCacheSandboxedFileFactory::CreateFilesAsync(
    const CacheIdString& cache_id,
    const std::string& product,
    CreateFilesCallback callback) {
  // The reply will be posted to the current SequencedTaskRunner.
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PersistentCacheSandboxedFileFactory::CreateFiles, this,
                     cache_id, product),
      std::move(callback));
}

bool PersistentCacheSandboxedFileFactory::ClearFiles(
    const CacheIdString& cache_id,
    const std::string& product) {
  DCHECK(!cache_root_dir_.empty());

  auto paths = GetPersistentCacheFilePaths(cache_root_dir_, cache_id, product);

  // Delete the whole version directory.
  DCHECK_EQ(paths.db_path.DirName(), paths.journal_path.DirName());
  return base::DeletePathRecursively(paths.db_path.DirName());
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

}  // namespace viz
