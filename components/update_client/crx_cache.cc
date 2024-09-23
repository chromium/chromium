// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_cache.h"

#include <cstdint>
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
#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"

namespace update_client {

CrxCache::CrxCache(const CrxCache::Options& options)
    : crx_cache_root_path_(options.crx_cache_root_path) {}

CrxCache::~CrxCache() = default;

CrxCache::Options::Options(const base::FilePath& crx_cache_root_path)
    : crx_cache_root_path(crx_cache_root_path) {}

base::FilePath CrxCache::BuildCrxFilePath(const std::string& id,
                                          const std::string& fp) {
  return crx_cache_root_path_.AppendASCII(base::JoinString({id, fp}, "_"));
}

bool CrxCache::Contains(const std::string& id, const std::string& fp) {
  return base::PathExists(BuildCrxFilePath(id, fp));
}

void CrxCache::Get(const std::string& id,
                   const std::string& fp,
                   base::OnceCallback<void(const Result& result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CrxCache::ProcessGet, this, id, fp),
      base::BindOnce(&CrxCache::EndRequest, this, std::move(callback)));
}

CrxCache::Result CrxCache::ProcessGet(const std::string& id,
                                      const std::string& fp) {
  CrxCache::Result result;
  if (!Contains(id, fp)) {
    result.error = UnpackerError::kPuffinMissingPreviousCrx;
  } else {
    result.error = UnpackerError::kNone;
    result.crx_cache_path = BuildCrxFilePath(id, fp);
  }
  return result;
}

void CrxCache::Put(const base::FilePath& crx_path,
                   const std::string& id,
                   const std::string& fp,
                   base::OnceCallback<void(const Result& result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CrxCache::ProcessPut, this, crx_path, id, fp),
      base::BindOnce(&CrxCache::EndRequest, this, std::move(callback)));
}

CrxCache::Result CrxCache::ProcessPut(const base::FilePath& crx_path,
                                      const std::string& id,
                                      const std::string& fp) {
  CrxCache::Result result;
  if (id.empty() || fp.empty()) {
    result.error = UnpackerError::kInvalidParams;
    return result;
  }
  base::FilePath dest_path = BuildCrxFilePath(id, fp);
  if (crx_path == dest_path) {
    result.error = UnpackerError::kNone;
    result.crx_cache_path = dest_path;
    return result;
  }
  RemoveAll(id);
  result.error = MoveFileToCache(crx_path, dest_path);
  if (result.error == UnpackerError::kNone) {
    result.crx_cache_path = dest_path;
  }
  return result;
}

void CrxCache::RemoveAll(const std::string& id) {
  base::FileEnumerator(crx_cache_root_path_, false, base::FileEnumerator::FILES,
                       [&id] {
                         const std::string result = base::StrCat({id, "*"});
#if BUILDFLAG(IS_WIN)
                         return base::UTF8ToWide(result);
#else
            return result;
#endif
                       }())
      .ForEach(
          [](const base::FilePath& file_path) { base::DeleteFile(file_path); });
}

UnpackerError CrxCache::MoveFileToCache(const base::FilePath& src_path,
                                        const base::FilePath& dest_path) {
  if (!base::CreateDirectory(crx_cache_root_path_)) {
    return update_client::UnpackerError::kFailedToCreateCacheDir;
  }
  if (!base::Move(src_path, dest_path)) {
    return update_client::UnpackerError::kFailedToAddToCache;
  }
  return update_client::UnpackerError::kNone;
}

void CrxCache::EndRequest(
    base::OnceCallback<void(const Result& result)> callback,
    CrxCache::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

}  // namespace update_client
