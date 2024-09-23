// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/cleanup_orphaned_isolated_web_apps_command.h"

#include <iterator>
#include <optional>
#include <set>
#include <variant>

#include "base/check_deref.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/uma_logging.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

namespace {
std::set<base::FilePath> RetrieveAllInstalledIsolatedWebAppsPaths(
    AllAppsLock& lock,
    const Profile& profile) {
  std::set<base::FilePath> isolated_web_apps_paths;
  const WebAppRegistrar& registrar = lock.registrar();
  for (const webapps::AppId& app_id : registrar.GetAppIds()) {
    const WebApp& app = CHECK_DEREF(registrar.GetAppById(app_id));
    if (const auto& isolation_data = app.isolation_data()) {
      const auto* owned_bundle =
          absl::get_if<IsolatedWebAppStorageLocation::OwnedBundle>(
              &isolation_data->location().variant());
      if (!owned_bundle) {
        continue;
      }

      isolated_web_apps_paths.insert(
          owned_bundle->GetPath(profile.GetPath()).DirName());

      if (const auto& pending_update_info =
              isolation_data->pending_update_info()) {
        const auto* pending_update_location =
            absl::get_if<IsolatedWebAppStorageLocation::OwnedBundle>(
                &pending_update_info->location.variant());
        if (pending_update_location) {
          isolated_web_apps_paths.insert(
              pending_update_location->GetPath(profile.GetPath()).DirName());
        }
      }
    }
  }
  return isolated_web_apps_paths;
}

std::set<base::FilePath> RetrieveAllIsolatedWebAppsDirectories(
    const base::FilePath& profile_dir) {
  const base::FilePath iwa_dir_path = profile_dir.Append(kIwaDirName);
  if (!base::DirectoryExists(iwa_dir_path)) {
    return {};
  }

  std::set<base::FilePath> isolated_app_directories;
  base::FileEnumerator enumerator(iwa_dir_path, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    isolated_app_directories.emplace(std::move(path));
  }
  return isolated_app_directories;
}

bool DeleteOrphanedIsolatedWebApps(std::vector<base::FilePath> paths) {
  return base::ranges::all_of(paths, [](const base::FilePath& path) {
    return base::DeletePathRecursively(path);
  });
}

base::expected<CleanupOrphanedIsolatedWebAppsCommandSuccess,
               CleanupOrphanedIsolatedWebAppsCommandError>
GetResult(int number_of_deleted_directories, bool success) {
  if (success) {
    return CleanupOrphanedIsolatedWebAppsCommandSuccess(
        /*number_of_cleaned_up_directories=*/number_of_deleted_directories);
  }
  return base::unexpected(CleanupOrphanedIsolatedWebAppsCommandError{
      .type = CleanupOrphanedIsolatedWebAppsCommandError::Type::
          kCouldNotDeleteAllBundles,
      .message = "Could not delete all orphaned isolated web apps."});
}

void RecordOutcomeMetric(
    base::expected<CleanupOrphanedIsolatedWebAppsCommandSuccess,
                   CleanupOrphanedIsolatedWebAppsCommandError> result) {
  base::expected<
      void, CleanupOrphanedIsolatedWebAppsCommand::CleanupOrphanedIWAsUMAError>
      uma_result;

  if (result.has_value()) {
    uma_result = base::ok();
  } else {
    switch (result.error().type) {
      case CleanupOrphanedIsolatedWebAppsCommandError::Type::
          kCouldNotDeleteAllBundles:
        uma_result = base::unexpected(
            CleanupOrphanedIsolatedWebAppsCommand::CleanupOrphanedIWAsUMAError::
                kCantDeleteAllOrphanedApps);
        break;
      case CleanupOrphanedIsolatedWebAppsCommandError::Type::kSystemShutdown:
        uma_result =
            base::unexpected(CleanupOrphanedIsolatedWebAppsCommand::
                                 CleanupOrphanedIWAsUMAError::kSystemShutdown);
        break;
    }
  }

  web_app::UmaLogExpectedStatus("WebApp.Isolated.OrphanedBundlesCleanupJob",
                                uma_result);
}

}  // namespace

CleanupOrphanedIsolatedWebAppsCommandSuccess::
    CleanupOrphanedIsolatedWebAppsCommandSuccess(
        int number_of_cleaned_up_directories)
    : number_of_cleaned_up_directories(number_of_cleaned_up_directories) {}

CleanupOrphanedIsolatedWebAppsCommandSuccess::
    ~CleanupOrphanedIsolatedWebAppsCommandSuccess() = default;

CleanupOrphanedIsolatedWebAppsCommandSuccess::
    CleanupOrphanedIsolatedWebAppsCommandSuccess(
        const CleanupOrphanedIsolatedWebAppsCommandSuccess& other) = default;

std::ostream& operator<<(
    std::ostream& os,
    const CleanupOrphanedIsolatedWebAppsCommandSuccess& success) {
  return os << "CleanupOrphanedIsolatedWebAppsCommandSuccess "
            << base::Value::Dict().Set(
                   "number_of_cleaned_up_directories",
                   success.number_of_cleaned_up_directories);
}

std::ostream& operator<<(
    std::ostream& os,
    const CleanupOrphanedIsolatedWebAppsCommandError& error) {
  std::string type;
  switch (error.type) {
    case CleanupOrphanedIsolatedWebAppsCommandError::Type::
        kCouldNotDeleteAllBundles:
      type = "CouldNotDeleteAllBundles";
      break;
    case CleanupOrphanedIsolatedWebAppsCommandError::Type::kSystemShutdown:
      type = "SystemShutdown";
  }
  return os << base::Value::Dict()
                   .Set("message", error.message)
                   .Set("type", type)
                   .DebugString();
}

CleanupOrphanedIsolatedWebAppsCommand::CleanupOrphanedIsolatedWebAppsCommand(
    Profile& profile,
    Callback callback)
    : WebAppCommand<AllAppsLock,
                    base::expected<CleanupOrphanedIsolatedWebAppsCommandSuccess,
                                   CleanupOrphanedIsolatedWebAppsCommandError>>(
          "CleanupOrphanedIsolatedWebAppsCommand",
          AllAppsLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/
          base::unexpected(CleanupOrphanedIsolatedWebAppsCommandError{
              .type = CleanupOrphanedIsolatedWebAppsCommandError::Type::
                  kSystemShutdown,
              .message = std::string("System shutting down.")})),
      profile_(profile) {}

CleanupOrphanedIsolatedWebAppsCommand::
    ~CleanupOrphanedIsolatedWebAppsCommand() = default;

void CleanupOrphanedIsolatedWebAppsCommand::StartWithLock(
    std::unique_ptr<AllAppsLock> lock) {
  CHECK(lock);
  lock_ = std::move(lock);

  const base::FilePath profile_dir = profile_->GetPath();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RetrieveAllIsolatedWebAppsDirectories, profile_dir),
      base::BindOnce(&CleanupOrphanedIsolatedWebAppsCommand::
                         OnIsolatedWebAppsDirectoriesRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CleanupOrphanedIsolatedWebAppsCommand::
    OnIsolatedWebAppsDirectoriesRetrieved(
        std::set<base::FilePath> isolated_web_apps_directories) {
  CHECK(lock_);
  std::set<base::FilePath> installed_isolated_web_apps_paths =
      RetrieveAllInstalledIsolatedWebAppsPaths(*lock_, *profile_);

  std::vector<base::FilePath> directories_to_delete;
  base::ranges::set_difference(isolated_web_apps_directories,
                               installed_isolated_web_apps_paths,
                               std::back_inserter(directories_to_delete));

  number_of_deleted_directories_ = directories_to_delete.size();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DeleteOrphanedIsolatedWebApps,
                     std::move(directories_to_delete)),
      base::BindOnce(&CleanupOrphanedIsolatedWebAppsCommand::CommandComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CleanupOrphanedIsolatedWebAppsCommand::CommandComplete(bool success) {
  auto result = GetResult(number_of_deleted_directories_, success);

  RecordOutcomeMetric(result);

  CompleteAndSelfDestruct(
      success ? CommandResult::kSuccess : CommandResult::kFailure, result);
}

}  // namespace web_app
