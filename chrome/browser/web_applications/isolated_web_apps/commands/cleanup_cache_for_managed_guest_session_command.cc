// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/cleanup_cache_for_managed_guest_session_command.h"

#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app {

namespace {

// This function is blocking, should be called only by
// `CleanupCacheForManagedGuestSessionCommand::StartWithLock`.
CleanupCacheForManagedGuestSessionResult CleanupCacheForManagedGuestSessionImpl(
    const std::vector<web_package::SignedWebBundleId>& iwas_to_keep_in_cache) {
  const base::FilePath cache_dir = GetManagedGuestSessionBundleCacheDirectory();

  std::vector<base::FilePath> dirs_to_keep =
      base::ToVector(iwas_to_keep_in_cache,
                     [&cache_dir](const web_package::SignedWebBundleId& id) {
                       return GetCacheBundleDirectory(cache_dir, id);
                     });

  // Remove all other caches except `dirs_to_keep`.
  std::vector<base::FilePath> dirs_to_delete;
  base::FileEnumerator all_iwa_dirs_iter(cache_dir, /*recursive=*/false,
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
    return base::ok(
        CleanupCacheForManagedGuestSessionSuccess{dirs_to_delete.size()});
  }

  return base::unexpected(CleanupCacheForManagedGuestSessionError{
      CleanupCacheForManagedGuestSessionError::Type::kCouldNotDeleteAllBundles,
      failed_to_cleaned_up_directories});
}

std::string CleanupCacheForManagedGuestSessionCommandErrorToString(
    const CleanupCacheForManagedGuestSessionError& error) {
  switch (error.type()) {
    case CleanupCacheForManagedGuestSessionError::Type::
        kCouldNotDeleteAllBundles:
      return "Could not delete bundles, number of failed directories: " +
             base::NumberToString(
                 error.number_of_failed_to_cleaned_up_directories());
    case CleanupCacheForManagedGuestSessionError::Type::kSystemShutdown:
      return "System is shutting down";
  }
}

}  // namespace

bool ShouldCleanupManagedGuestSessionCache() {
  return IsIwaBundleCacheEnabled() && chromeos::IsManagedGuestSession();
}

CleanupCacheForManagedGuestSessionCommand::
    CleanupCacheForManagedGuestSessionCommand(
        const std::vector<web_package::SignedWebBundleId>&
            iwas_to_keep_in_cache,
        Callback callback)
    : WebAppCommand<AllAppsLock, CleanupCacheForManagedGuestSessionResult>(
          "CleanupCacheForManagedGuestSessionCommand",
          AllAppsLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/
          base::unexpected(CleanupCacheForManagedGuestSessionError{
              CleanupCacheForManagedGuestSessionError::Type::kSystemShutdown})),
      iwas_to_keep_in_cache_(iwas_to_keep_in_cache) {
  CHECK(ShouldCleanupManagedGuestSessionCache());
}

CleanupCacheForManagedGuestSessionCommand::
    ~CleanupCacheForManagedGuestSessionCommand() = default;

void CleanupCacheForManagedGuestSessionCommand::StartWithLock(
    std::unique_ptr<AllAppsLock> lock) {
  CHECK(lock);
  lock_ = std::move(lock);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CleanupCacheForManagedGuestSessionImpl,
                     iwas_to_keep_in_cache_),
      base::BindOnce(
          &CleanupCacheForManagedGuestSessionCommand::CommandComplete,
          weak_ptr_factory_.GetWeakPtr()));
}

void CleanupCacheForManagedGuestSessionCommand::CommandComplete(
    const CleanupCacheForManagedGuestSessionResult& result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Cleanup cache for Managed Guest Session failed: "
               << CleanupCacheForManagedGuestSessionCommandErrorToString(
                      result.error());
  }
  CompleteAndSelfDestruct(
      result.has_value() ? CommandResult::kSuccess : CommandResult::kFailure,
      result);
}

}  // namespace web_app
