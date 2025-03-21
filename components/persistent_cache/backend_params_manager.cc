// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/backend_params_manager.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"

namespace {

constexpr const char kPathSeparator[] = "_";
constexpr const char kDbFile[] = "db_file";
constexpr const char kJournalFile[] = "journal_file";

constexpr size_t kLruCacheCapacity = 100;
}  // namespace

namespace persistent_cache {

BackendParamsManager::BackendParamsManager(base::FilePath top_directory)
    : backend_params_map_(kLruCacheCapacity),
      top_directory_(std::move(top_directory)) {}
BackendParamsManager::~BackendParamsManager() = default;

void BackendParamsManager::GetParamsSyncOrCreateAsync(
    BackendType backend_type,
    const std::string& key,
    CompletedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = backend_params_map_.Get(
      BackendParamsKey{.backend_type = backend_type, .key = key});
  if (it != backend_params_map_.end()) {
    std::move(callback).Run(it->second);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&BackendParamsManager::CreateParamsSync, top_directory_,
                     backend_type, key),
      base::BindOnce(&BackendParamsManager::SaveParams,
                     weak_factory_.GetWeakPtr(), key, std::move(callback)));
}

// static
BackendParams BackendParamsManager::CreateParamsSync(base::FilePath directory,
                                                     BackendType backend_type,
                                                     const std::string& key) {
  BackendParams params;
  params.type = backend_type;

  uint32_t flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                   base::File::FLAG_WRITE;
  params.db_file = base::File(
      directory.AppendASCII(base::StrCat({key, kPathSeparator, kDbFile})),
      flags);
  params.db_file_is_writable = true;

  params.journal_file = base::File(
      directory.AppendASCII(base::StrCat({key, kPathSeparator, kJournalFile})),
      flags);
  params.journal_file_is_writable = true;

  return params;
}

void BackendParamsManager::SaveParams(const std::string& key,
                                      CompletedCallback callback,
                                      BackendParams backend_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(callback).Run(backend_params);

  // Avoid saving invalid files.
  if (backend_params.db_file.IsValid() &&
      backend_params.journal_file.IsValid()) {
    backend_params_map_.Put(
        BackendParamsKey{.backend_type = backend_params.type, .key = key},
        std::move(backend_params));
  }
}

}  // namespace persistent_cache
