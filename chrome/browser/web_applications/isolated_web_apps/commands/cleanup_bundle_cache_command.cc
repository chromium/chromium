// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/cleanup_bundle_cache_command.h"

#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/error/uma_logging.h"

namespace web_app {

namespace {

using SessionType = IwaCacheClient::SessionType;

constexpr char kCleanupBundleCacheMetric[] =
    "WebApp.Isolated.CleanupBundleCache";

// This function is blocking, should be called only by
// `CleanupBundleCacheCommand::StartWithLock`.
CleanupBundleCacheResult CleanupBundleCacheCommandImpl(
    const std::vector<web_package::SignedWebBundleId>& iwas_to_keep_in_cache,
    SessionType session_type) {
  const base::FilePath cache_base_dir =
      IwaCacheClient::GetCacheBaseDirectoryForSessionType(session_type);

  std::vector<base::FilePath> dirs_to_keep = base::ToVector(
      iwas_to_keep_in_cache,
      [&cache_base_dir](const web_package::SignedWebBundleId& id) {
        return IwaCacheClient::GetCacheDirectoryForBundle(cache_base_dir, id);
      });

  // Remove all other caches except `dirs_to_keep`.
  std::vector<base::FilePath> dirs_to_delete;
  base::FileEnumerator all_iwa_dirs_iter(cache_base_dir, /*recursive=*/false,
                                         base::FileEnumerator::DIRECTORIES);
  all_iwa_dirs_iter.ForEach(
      [&dirs_to_keep, &dirs_to_delete](const base::FilePath& dir_path) {
        if (!base::Contains(dirs_to_keep, dir_path)) {
          dirs_to_delete.push_back(dir_path);
        }
      });

  size_t failed_to_cleaned_up_directories = 0;
  for (const base::FilePath& dir_to_delete : dirs_to_delete) {
    if (!base::DeletePathRecursively(dir_to_delete)) {
      ++failed_to_cleaned_up_directories;
      LOG(ERROR) << "Failed to delete IWA bundle cached directory, path: "
                 << dir_to_delete;
    }
  }
  if (failed_to_cleaned_up_directories == 0) {
    return base::ok(CleanupBundleCacheSuccess{dirs_to_delete.size()});
  }

  return base::unexpected(CleanupBundleCacheError{
      CleanupBundleCacheError::Type::kCouldNotDeleteAllBundles,
      failed_to_cleaned_up_directories});
}

CleanupBundleCacheResult RecordMetric(CleanupBundleCacheResult result) {
  web_app::UmaLogExpectedStatus(
      kCleanupBundleCacheMetric,
      result.transform_error(&CleanupBundleCacheError::type));
  return result;
}

}  // namespace

std::string CleanupBundleCacheSuccess::ToString() const {
  return "Successfully finished cleanup, number of cleaned up directories: " +
         base::NumberToString(number_of_cleaned_up_directories_);
}

std::string CleanupBundleCacheError::ToString() const {
  std::string result = "Failed to finish cleanup: ";
  switch (type_) {
    case CleanupBundleCacheError::Type::kCouldNotDeleteAllBundles:
      result +=
          "Could not delete bundles, number of failed directories: " +
          base::NumberToString(number_of_failed_to_cleaned_up_directories_);
      break;
    case CleanupBundleCacheError::Type::kSystemShutdown:
      result += "System is shutting down";
      break;
  }
  return result;
}

CleanupBundleCacheCommand::CleanupBundleCacheCommand(
    const std::vector<web_package::SignedWebBundleId>& iwas_to_keep_in_cache,
    SessionType session_type,
    Callback callback)
    : WebAppCommand<AllAppsLock, CleanupBundleCacheResult>(
          "CleanupBundleCacheCommand",
          AllAppsLockDescription(),
          base::BindOnce(&RecordMetric).Then(std::move(callback)),
          /*args_for_shutdown=*/
          base::unexpected(CleanupBundleCacheError{
              CleanupBundleCacheError::Type::kSystemShutdown})),
      iwas_to_keep_in_cache_(iwas_to_keep_in_cache),
      session_type_(session_type) {}

CleanupBundleCacheCommand::~CleanupBundleCacheCommand() = default;

void CleanupBundleCacheCommand::StartWithLock(
    std::unique_ptr<AllAppsLock> lock) {
  CHECK(lock);
  lock_ = std::move(lock);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CleanupBundleCacheCommandImpl, iwas_to_keep_in_cache_,
                     session_type_),
      base::BindOnce(&CleanupBundleCacheCommand::CommandComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CleanupBundleCacheCommand::CommandComplete(
    const CleanupBundleCacheResult& result) {
  CompleteAndSelfDestruct(
      result.has_value() ? CommandResult::kSuccess : CommandResult::kFailure,
      result);
}

}  // namespace web_app
