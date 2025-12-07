// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/copy_bundle_to_cache_command.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"

namespace web_app {
namespace {

using SessionType = IwaCacheClient::SessionType;

// This function is blocking. It should be called by
// `CopyBundleToCacheCommand::StartWithLock`.
CopyBundleToCacheResult CopyBundleToCacheCommandImpl(
    const base::FilePath& copy_from_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    IwaVersion version,
    SessionType session_type) {
  const base::FilePath cache_dir =
      IwaCacheClient::GetCacheBaseDirectoryForSessionType(session_type);
  base::FilePath bundle_dir_with_version =
      IwaCacheClient::GetCacheDirectoryForBundleWithVersion(
          cache_dir, web_bundle_id, version);
  if (base::File::Error error;
      !base::CreateDirectoryAndGetError(bundle_dir_with_version, &error)) {
    LOG(ERROR) << "Failed to create IWA cache directory with path: "
               << bundle_dir_with_version << ", error: " << error;
    return base::unexpected(CopyBundleToCacheError::kFailedToCreateDir);
  }

  const base::FilePath destination_bundle_path =
      IwaCacheClient::GetBundleFullName(bundle_dir_with_version);
  if (!base::CopyFile(copy_from_bundle_path, destination_bundle_path)) {
    base::DeleteFile(destination_bundle_path);
    LOG(ERROR) << "Failed to copy IWA bundle to cache, destination path: "
               << destination_bundle_path;
    return base::unexpected(CopyBundleToCacheError::kFailedToCopyFile);
  }

  return CopyBundleToCacheSuccess(destination_bundle_path);
}

// Returns bundle path for owned bundle, otherwise returns std::nullopt.
std::optional<base::FilePath> GetOwnedBundlePath(
    const IsolatedWebAppStorageLocation& location,
    Profile& profile) {
  const auto* owned_bundle =
      std::get_if<IsolatedWebAppStorageLocation::OwnedBundle>(
          &location.variant());
  if (!owned_bundle) {
    return std::nullopt;
  }
  return owned_bundle->GetPath(profile.GetPath());
}

}  // namespace

std::string CopyBundleToCacheErrorToString(CopyBundleToCacheError error) {
  switch (error) {
    case CopyBundleToCacheError::kSystemShutdown:
      return "System shutdown";
    case CopyBundleToCacheError::kAppNotInstalled:
      return "IWA is not installed";
    case CopyBundleToCacheError::kNotIwa:
      return "App is not IWA";
    case CopyBundleToCacheError::kCannotExtractOwnedBundlePath:
      return "Cannot extract owned bundle path";
    case CopyBundleToCacheError::kFailedToCreateDir:
      return "Failed to create directory";
    case CopyBundleToCacheError::kFailedToCopyFile:
      return "Failed to copy file";
  }
}

CopyBundleToCacheCommand::CopyBundleToCacheCommand(
    const IsolatedWebAppUrlInfo& url_info,
    SessionType session_type,
    Profile& profile,
    Callback callback)
    : WebAppCommand<AppLock, CopyBundleToCacheResult>(
          "CopyBundleToCacheCommand",
          AppLockDescription(url_info.app_id()),
          std::move(callback),
          /*args_for_shutdown=*/
          base::unexpected(
              CopyBundleToCacheError{CopyBundleToCacheError::kSystemShutdown})),
      url_info_(url_info),
      session_type_(session_type),
      profile_(profile) {}

CopyBundleToCacheCommand::~CopyBundleToCacheCommand() = default;

void CopyBundleToCacheCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  CHECK(lock);
  lock_ = std::move(lock);

  const WebApp* app = lock_->registrar().GetAppById(url_info_.app_id());
  if (!app) {
    CommandComplete(base::unexpected(
        CopyBundleToCacheError{CopyBundleToCacheError::kAppNotInstalled}));
    return;
  }
  if (!app->isolation_data()) {
    CommandComplete(base::unexpected(
        CopyBundleToCacheError{CopyBundleToCacheError::kNotIwa}));
    return;
  }

  // Only copies owned bundles, since all policy-installed IWAs have owned
  // bundles.
  std::optional<base::FilePath> bundle_path =
      GetOwnedBundlePath(app->isolation_data()->location(), *profile_);
  if (bundle_path->empty()) {
    CommandComplete(base::unexpected(CopyBundleToCacheError{
        CopyBundleToCacheError::kCannotExtractOwnedBundlePath}));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CopyBundleToCacheCommandImpl, bundle_path.value(),
                     url_info_.web_bundle_id(),
                     app->isolation_data()->version(), session_type_),
      base::BindOnce(&CopyBundleToCacheCommand::CommandComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CopyBundleToCacheCommand::CommandComplete(
    const CopyBundleToCacheResult& result) {
  CompleteAndSelfDestruct(
      result.has_value() ? CommandResult::kSuccess : CommandResult::kFailure,
      result);
}

}  // namespace web_app
