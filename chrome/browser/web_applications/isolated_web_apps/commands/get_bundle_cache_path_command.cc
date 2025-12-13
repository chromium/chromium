// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/get_bundle_cache_path_command.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "components/webapps/isolated_web_apps/error/uma_logging.h"
namespace web_app {

using SessionType = IwaCacheClient::SessionType;

constexpr char kGetBundleCachePathMetric[] =
    "WebApp.Isolated.GetBundleCachePath";

namespace {

base::FilePath GetBundleFullName(
    const base::FilePath& cache_dir,
    const web_package::SignedWebBundleId& web_bundle_id,
    const IwaVersion& version) {
  return IwaCacheClient::GetBundleFullName(
      IwaCacheClient::GetCacheDirectoryForBundleWithVersion(
          cache_dir, web_bundle_id, version));
}

// Expects the following bundle path:
// "/var/cache/device_local_account_iwa/<mgs|kiosk>/<bundle_id>/<version>/" +
//   "main.swbn"
// Returns `std::nullopt` if version cannot be parsed.
std::optional<IwaVersion> ExtractVersionFromCacheBundlePath(
    const base::FilePath& file) {
  static constexpr int kVersionOffsetInPath = 2;

  std::vector<base::FilePath::StringType> components = file.GetComponents();
  if (components.size() >= kVersionOffsetInPath &&
      file.Extension() == ".swbn") {
    const auto iwa_version = IwaVersion::Create(
        components[components.size() - kVersionOffsetInPath]);
    if (iwa_version.has_value()) {
      return *std::move(iwa_version);
    }
  }
  LOG(ERROR) << "Cannot extract bundle version from path: " << file;
  return std::nullopt;
}

// This function is blocking. It should be called by
// `GetBundleCachePathCommand::StartWithLock`.
GetBundleCachePathResult GetBundleCachePathImpl(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::optional<IwaVersion>& version,
    SessionType session_type) {
  const base::FilePath cache_dir =
      IwaCacheClient::GetCacheBaseDirectoryForSessionType(session_type);

  if (version) {
    base::FilePath expected_file_path =
        GetBundleFullName(cache_dir, web_bundle_id, *version);
    if (base::PathIsReadable(expected_file_path)) {
      return GetBundleCachePathSuccess(std::move(expected_file_path),
                                       *std::move(version));
    }
    return base::unexpected(GetBundleCachePathError::kProvidedVersionNotFound);
  }
  // When `version` is not provided, take the latest cached version.
  base::FilePath bundle_dir =
      IwaCacheClient::GetCacheDirectoryForBundle(cache_dir, web_bundle_id);
  base::FileEnumerator bundle_files_iter(bundle_dir, /*recursive=*/true,
                                         base::FileEnumerator::FILES);

  std::optional<base::FilePath> newest_version_path = std::nullopt;
  std::optional<IwaVersion> newest_version = std::nullopt;
  bundle_files_iter.ForEach([&newest_version_path, &newest_version](
                                const base::FilePath& current_path) {
    std::optional<IwaVersion> current_version =
        ExtractVersionFromCacheBundlePath(current_path);

    if (newest_version < current_version) {
      newest_version = std::move(current_version);
      newest_version_path = current_path;
    }
  });

  if (!newest_version_path) {
    return base::unexpected(GetBundleCachePathError::kIwaNotCached);
  }

  return GetBundleCachePathSuccess(std::move(newest_version_path.value()),
                                   std::move(newest_version.value()));
}

GetBundleCachePathResult RecordMetric(GetBundleCachePathResult result) {
  web_app::UmaLogExpectedStatus(kGetBundleCachePathMetric, result);
  return result;
}

}  // namespace

std::string GetBundleCachePathErrorToString(GetBundleCachePathError error) {
  switch (error) {
    case GetBundleCachePathError::kSystemShutdown:
      return "System shutdown";
    case GetBundleCachePathError::kProvidedVersionNotFound:
      return "Provided version of IWA not found in cache";
    case GetBundleCachePathError::kIwaNotCached:
      return "IWA not cached";
  }
}

GetBundleCachePathCommand::GetBundleCachePathCommand(
    const IsolatedWebAppUrlInfo& url_info,
    const std::optional<IwaVersion>& version,
    SessionType session_type,
    Callback callback)
    : WebAppCommand<AppLock, GetBundleCachePathResult>(
          "GetBundleCachePathCommand",
          AppLockDescription(url_info.app_id()),
          base::BindOnce(&RecordMetric).Then(std::move(callback)),
          /*args_for_shutdown=*/
          base::unexpected(GetBundleCachePathError{
              GetBundleCachePathError::kSystemShutdown})),
      url_info_(url_info),
      version_(version),
      session_type_(session_type) {}

GetBundleCachePathCommand::~GetBundleCachePathCommand() = default;

void GetBundleCachePathCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  CHECK(lock);
  lock_ = std::move(lock);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetBundleCachePathImpl, url_info_.web_bundle_id(),
                     version_, session_type_),
      base::BindOnce(&GetBundleCachePathCommand::CommandComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GetBundleCachePathCommand::CommandComplete(
    const GetBundleCachePathResult& result) {
  CompleteAndSelfDestruct(
      result.has_value() ? CommandResult::kSuccess : CommandResult::kFailure,
      result);
}

}  // namespace web_app
