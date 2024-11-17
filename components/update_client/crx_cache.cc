// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_cache.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace update_client {
namespace {

void CleanUp(const base::FilePath& path, const std::string& id) {
  base::FileEnumerator(path, false, base::FileEnumerator::FILES,
#if BUILDFLAG(IS_WIN)
                       base::UTF8ToWide(base::StrCat({id, "*"}))
#else
                       base::StrCat({id, "*"})
#endif
                           )
      .ForEach(
          [](const base::FilePath& file_path) { base::DeleteFile(file_path); });
}

}  // namespace

CrxCache::CrxCache(std::optional<base::FilePath> crx_cache_root_path)
    : crx_cache_root_path_(crx_cache_root_path) {}

CrxCache::~CrxCache() = default;

base::FilePath CrxCache::BuildCrxFilePath(const std::string& id,
                                          const std::string& fp) {
  CHECK(crx_cache_root_path_);
  return crx_cache_root_path_->AppendASCII(base::JoinString({id, fp}, "_"));
}

void CrxCache::Get(
    const std::string& id,
    const std::string& fp,
    base::OnceCallback<
        void(const base::expected<base::FilePath, UnpackerError>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!crx_cache_root_path_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected(UnpackerError::kCrxCacheNotProvided)));
    return;
  }
  const base::FilePath path = BuildCrxFilePath(id, fp);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path)
              -> base::expected<base::FilePath, UnpackerError> {
            if (base::PathExists(path)) {
              return path;
            }
            return base::unexpected(UnpackerError::kPuffinMissingPreviousCrx);
          },
          path),
      std::move(callback));
}

void CrxCache::Put(
    const base::FilePath& src,
    const std::string& id,
    const std::string& fp,
    base::OnceCallback<
        void(const base::expected<base::FilePath, UnpackerError>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!crx_cache_root_path_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected(UnpackerError::kCrxCacheNotProvided)));
    return;
  }
  const base::FilePath dest = BuildCrxFilePath(id, fp);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& src, const base::FilePath& dest,
             const std::string& id)
              -> base::expected<base::FilePath, UnpackerError> {
            // If already in cache, just return the path.
            if (src == dest) {
              return dest;
            }

            // Delete existing files for the app.
            CleanUp(dest.DirName(), id);

            // Move the file into cache.
            if (!base::CreateDirectory(dest.DirName())) {
              return base::unexpected(UnpackerError::kFailedToCreateCacheDir);
            }
            if (!base::Move(src, dest)) {
              return base::unexpected(UnpackerError::kFailedToAddToCache);
            }
            return dest;
          },
          src, dest, id),
      std::move(callback));
}

void CrxCache::RemoveAll(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!crx_cache_root_path_) {
    return;
  }
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&CleanUp, *crx_cache_root_path_, id));
}

}  // namespace update_client
