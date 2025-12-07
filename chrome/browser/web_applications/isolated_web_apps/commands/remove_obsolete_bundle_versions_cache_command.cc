// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/remove_obsolete_bundle_versions_cache_command.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/isolated_web_apps/error/uma_logging.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"

namespace web_app {

namespace {

using SessionType = IwaCacheClient::SessionType;

constexpr char kRemoveObsoleteBundleVersionsMetric[] =
    "WebApp.Isolated.RemoveObsoleteBundleVersions";

RemoveObsoleteBundleVersionsResult RemoveObsoleteBundleVersionsCacheCommandImpl(
    const web_package::SignedWebBundleId& web_bundle_id,
    IwaVersion installed_version,
    SessionType session_type) {
  const base::FilePath cache_base_dir =
      IwaCacheClient::GetCacheBaseDirectoryForSessionType(session_type);

  base::FilePath currently_installed_version_cache_dir =
      IwaCacheClient::GetCacheDirectoryForBundleWithVersion(
          cache_base_dir, web_bundle_id, installed_version);

  if (!base::PathIsReadable(currently_installed_version_cache_dir)) {
    return base::unexpected(RemoveObsoleteBundleVersionsError{
        RemoveObsoleteBundleVersionsError::Type::kInstalledVersionNotCached});
  }

  // Remove all other cached IWA versions except
  // `currently_installed_version_cache_dir`.
  std::vector<base::FilePath> versions_dirs_to_delete;
  base::FileEnumerator all_iwa_versions_dirs_iter(
      IwaCacheClient::GetCacheDirectoryForBundle(cache_base_dir, web_bundle_id),
      /*recursive=*/false, base::FileEnumerator::DIRECTORIES);
  all_iwa_versions_dirs_iter.ForEach(
      [&currently_installed_version_cache_dir,
       &versions_dirs_to_delete](const base::FilePath& dir_path) {
        if (dir_path != currently_installed_version_cache_dir) {
          versions_dirs_to_delete.push_back(dir_path);
        }
      });

  size_t failed_to_remove_versions = 0;
  for (const base::FilePath& dir_to_delete : versions_dirs_to_delete) {
    if (!base::DeletePathRecursively(dir_to_delete)) {
      ++failed_to_remove_versions;
    }
  }
  if (failed_to_remove_versions == 0) {
    return base::ok(
        RemoveObsoleteBundleVersionsSuccess{versions_dirs_to_delete.size()});
  }

  return base::unexpected(RemoveObsoleteBundleVersionsError{
      RemoveObsoleteBundleVersionsError::Type::kCouldNotDeleteAllVersions,
      failed_to_remove_versions});
}

base::expected<IwaVersion, RemoveObsoleteBundleVersionsError> GetIwaVersion(
    WebAppRegistrar& registrar,
    const webapps::AppId& app_id) {
  const WebApp* app = registrar.GetAppById(app_id);
  if (!app) {
    return base::unexpected(RemoveObsoleteBundleVersionsError{
        RemoveObsoleteBundleVersionsError::Type::kAppNotInstalled});
  }

  return app->isolation_data().value().version();
}

RemoveObsoleteBundleVersionsResult RecordMetric(
    RemoveObsoleteBundleVersionsResult result) {
  web_app::UmaLogExpectedStatus(
      kRemoveObsoleteBundleVersionsMetric,
      result.transform_error(&RemoveObsoleteBundleVersionsError::type));
  return result;
}

}  // namespace

std::string RemoveObsoleteBundleVersionsSuccess::ToString() const {
  return "Successfully finished versions cleanup, number of removed obsolete "
         "versions: " +
         base::NumberToString(number_of_removed_versions_);
}

std::string RemoveObsoleteBundleVersionsError::ToString() const {
  std::string result = "Failed to finish versions cleanup: ";
  switch (type_) {
    case RemoveObsoleteBundleVersionsError::Type::kSystemShutdown:
      result += "System is shutting down";
      break;
    case RemoveObsoleteBundleVersionsError::Type::kAppNotInstalled:
      result += "IWA is not installed";
      break;
    case RemoveObsoleteBundleVersionsError::Type::kInstalledVersionNotCached:
      result += "Installed version not cached";
      break;
    case RemoveObsoleteBundleVersionsError::Type::kCouldNotDeleteAllVersions:
      result +=
          "Could not delete all previous versions, number of failed versions "
          "to delete: " +
          base::NumberToString(number_of_failed_to_remove_versions_);
      break;
  }
  return result;
}

RemoveObsoleteBundleVersionsCacheCommand::
    RemoveObsoleteBundleVersionsCacheCommand(
        const IsolatedWebAppUrlInfo& url_info,
        IwaCacheClient::SessionType session_type,
        Callback callback)
    : WebAppCommand<AppLock, RemoveObsoleteBundleVersionsResult>(
          "RemoveObsoleteBundleVersionsCacheCommand",
          AppLockDescription(url_info.app_id()),
          base::BindOnce(&RecordMetric).Then(std::move(callback)),
          /*args_for_shutdown=*/
          base::unexpected(RemoveObsoleteBundleVersionsError{
              RemoveObsoleteBundleVersionsError::Type::kSystemShutdown})),
      url_info_(url_info),
      session_type_(session_type) {}

RemoveObsoleteBundleVersionsCacheCommand::
    ~RemoveObsoleteBundleVersionsCacheCommand() = default;

void RemoveObsoleteBundleVersionsCacheCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  CHECK(lock);
  lock_ = std::move(lock);

  ASSIGN_OR_RETURN(const IwaVersion installed_version,
                   GetIwaVersion(lock_->registrar(), url_info_.app_id()),
                   [&](const RemoveObsoleteBundleVersionsError& error) {
                     CommandComplete(base::unexpected(error));
                   });

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RemoveObsoleteBundleVersionsCacheCommandImpl,
                     url_info_.web_bundle_id(), installed_version,
                     session_type_),
      base::BindOnce(&RemoveObsoleteBundleVersionsCacheCommand::CommandComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoveObsoleteBundleVersionsCacheCommand::CommandComplete(
    const RemoveObsoleteBundleVersionsResult& result) {
  CompleteAndSelfDestruct(
      result.has_value() ? CommandResult::kSuccess : CommandResult::kFailure,
      result);
}

}  // namespace web_app
