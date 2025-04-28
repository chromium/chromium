// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_paths.h"
#include "base/containers/to_vector.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app {

namespace {

using SessionType = IwaCacheClient::SessionType;

base::FilePath GetCacheBundleDirectoryWithVersion(
    const base::FilePath& cache_dir,
    const web_package::SignedWebBundleId& web_bundle_id,
    const base::Version& version) {
  return IwaCacheClient::GetCacheDirectoryForBundle(cache_dir, web_bundle_id)
      .AppendASCII(version.GetString());
}

base::FilePath GetBundleFullName(
    const base::FilePath& bundle_dir_with_version) {
  return bundle_dir_with_version.AppendASCII(kMainSwbnFileName);
}

base::FilePath GetBundleFullName(
    const base::FilePath& cache_dir,
    const web_package::SignedWebBundleId& web_bundle_id,
    const base::Version& version) {
  return GetBundleFullName(
      GetCacheBundleDirectoryWithVersion(cache_dir, web_bundle_id, version));
}

// Expects the following bundle path:
// "/var/cache/device_local_account_iwa/<mgs|kiosk>/<bundle_id>/<version>/" +
//   "main.swbn"
// Returns `std::nullopt` if version cannot be parsed.
std::optional<base::Version> ExtractVersionFromCacheBundlePath(
    const base::FilePath& file) {
  static constexpr int kVersionOffsetInPath = 2;

  std::vector<base::FilePath::StringType> components = file.GetComponents();
  if (components.size() >= kVersionOffsetInPath &&
      file.Extension() == ".swbn") {
    base::Version version =
        base::Version(components[components.size() - kVersionOffsetInPath]);
    if (version.IsValid()) {
      return version;
    }
  }
  return std::nullopt;
}

// This function is blocking. It should be called by
// `GetCacheFilePath`.
std::optional<IwaCacheClient::CachedBundleData> GetCacheFilePathImpl(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::optional<base::Version>& version,
    const base::FilePath& cache_dir) {
  if (version) {
    base::FilePath expected_file_path =
        GetBundleFullName(cache_dir, web_bundle_id, version.value());
    if (base::PathIsReadable(expected_file_path)) {
      return IwaCacheClient::CachedBundleData(std::move(expected_file_path),
                                              std::move(version.value()));
    } else {
      return std::nullopt;
    }
  }
  // When `version` is not provided, take the latest cached version.
  base::FilePath bundle_dir =
      IwaCacheClient::GetCacheDirectoryForBundle(cache_dir, web_bundle_id);
  base::FileEnumerator bundle_files_iter(bundle_dir, /*recursive=*/true,
                                         base::FileEnumerator::FILES);

  std::optional<base::FilePath> newest_version_path = std::nullopt;
  std::optional<base::Version> newest_version = std::nullopt;
  bundle_files_iter.ForEach([&newest_version_path, &newest_version](
                                const base::FilePath& current_path) {
    std::optional<base::Version> current_version =
        ExtractVersionFromCacheBundlePath(current_path);

    if (newest_version < current_version) {
      newest_version = current_version;
      newest_version_path = current_path;
    }
  });

  if (!newest_version_path) {
    return std::nullopt;
  }

  return IwaCacheClient::CachedBundleData(
      std::move(newest_version_path.value()),
      std::move(newest_version.value()));
}

// This function is blocking. It should be called by `CopyBundleToCache`.
base::expected<IwaCacheClient::CopyBundleToCacheSuccess,
               IwaCacheClient::CopyBundleToCacheError>
CopyBundleToCacheImpl(const base::FilePath& copy_from_bundle_path,
                      const web_package::SignedWebBundleId& web_bundle_id,
                      base::Version version,
                      const base::FilePath& cache_dir) {
  base::FilePath bundle_dir_with_version =
      GetCacheBundleDirectoryWithVersion(cache_dir, web_bundle_id, version);
  if (base::File::Error error;
      !base::CreateDirectoryAndGetError(bundle_dir_with_version, &error)) {
    LOG(ERROR) << "Failed to create IWA cache directory with path: "
               << bundle_dir_with_version << ", error: " << error;
    return base::unexpected(
        IwaCacheClient::CopyBundleToCacheError::kFailedToCreateDir);
  }

  const base::FilePath destination_bundle_path =
      GetBundleFullName(bundle_dir_with_version);
  if (!base::CopyFile(copy_from_bundle_path, destination_bundle_path)) {
    base::DeleteFile(destination_bundle_path);
    LOG(ERROR) << "Failed to copy IWA bundle to cache, destination path: "
               << destination_bundle_path;
    return base::unexpected(
        IwaCacheClient::CopyBundleToCacheError::kFailedToCopyFile);
  }

  return IwaCacheClient::CopyBundleToCacheSuccess(destination_bundle_path);
}

base::FilePath GetIwaCacheDirectoryForCurrentSession(
    const base::FilePath& base = base::PathService::CheckedGet(
        ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE)) {
  if (chromeos::IsKioskSession()) {
    return IwaCacheClient::GetCacheBaseDirectoryForSessionType(
        SessionType::kKiosk, base);
  } else if (chromeos::IsManagedGuestSession()) {
    return IwaCacheClient::GetCacheBaseDirectoryForSessionType(
        SessionType::kManagedGuestSession, base);
  }
  NOTREACHED() << "Unsupported session type for IWA caching";
}

}  // namespace

bool IsIwaBundleCacheEnabled() {
  return base::FeatureList::IsEnabled(features::kIsolatedWebAppBundleCache) &&
         (chromeos::IsManagedGuestSession() || chromeos::IsKioskSession());
}

base::FilePath GetCacheBundleDirectory(
    const base::FilePath& main_cache_dir,
    const web_package::SignedWebBundleId& web_bundle_id) {
  return main_cache_dir.AppendASCII(web_bundle_id.id());
}

// static
std::string IwaCacheClient::CopyErrorToString(
    IwaCacheClient::CopyBundleToCacheError error) {
  switch (error) {
    case IwaCacheClient::CopyBundleToCacheError::kFailedToCreateDir:
      return "FailedToCreateDir";
    case IwaCacheClient::CopyBundleToCacheError::kFailedToCopyFile:
      return "FailedToCopyFile";
  }
}

IwaCacheClient::IwaCacheClient()
    : cache_dir_(GetIwaCacheDirectoryForCurrentSession()) {
  CHECK(IsIwaBundleCacheEnabled())
      << "IwaCacheClient should only be created "
         "inside mgs or kiosk sessions and when the feature is enabled";
}

void IwaCacheClient::GetCacheFilePath(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::optional<base::Version>& version,
    base::OnceCallback<void(std::optional<CachedBundleData>)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetCacheFilePathImpl, web_bundle_id, version, cache_dir_),
      std::move(callback));
}

void IwaCacheClient::CopyBundleToCache(
    const base::FilePath& copy_from_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    const base::Version& version,
    base::OnceCallback<void(base::expected<CopyBundleToCacheSuccess,
                                           CopyBundleToCacheError>)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CopyBundleToCacheImpl, copy_from_bundle_path,
                     web_bundle_id, version, cache_dir_),
      std::move(callback));
}

void IwaCacheClient::SetCacheDirForTesting(const base::FilePath& cache_dir) {
  cache_dir_ = GetIwaCacheDirectoryForCurrentSession(cache_dir);
}

// static
base::FilePath IwaCacheClient::GetCacheBaseDirectoryForSessionType(
    IwaCacheClient::SessionType session_type,
    const base::FilePath& base) {
  std::string_view session_dir;
  switch (session_type) {
    case SessionType::kKiosk:
      session_dir = IwaCacheClient::kKioskDirName;
      break;
    case SessionType::kManagedGuestSession:
      session_dir = IwaCacheClient::kMgsDirName;
      break;
  }
  return base.AppendASCII(session_dir);
}

// static
base::FilePath IwaCacheClient::GetCacheDirectoryForBundle(
    const base::FilePath& cache_base_dir,
    const web_package::SignedWebBundleId& web_bundle_id) {
  return cache_base_dir.AppendASCII(web_bundle_id.id());
}

// static
std::string IwaCacheClient::SessionTypeToString(SessionType session_type) {
  switch (session_type) {
    case SessionType::kKiosk:
      return "Kiosk";
    case SessionType::kManagedGuestSession:
      return "Managed Guest Session";
  }
}

}  // namespace web_app
