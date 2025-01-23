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
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app {

namespace {

base::FilePath GetCacheBundleDirectory(
    const base::FilePath& cache_dir,
    const web_package::SignedWebBundleId& web_bundle_id) {
  return cache_dir.AppendASCII(web_bundle_id.id());
}

base::FilePath GetCacheBundleDirectoryWithVersion(
    const base::FilePath& cache_dir,
    const web_package::SignedWebBundleId& web_bundle_id,
    const base::Version& version) {
  return GetCacheBundleDirectory(cache_dir, web_bundle_id)
      .AppendASCII(version.GetString());
}

base::FilePath GetBundleFullName(
    const base::FilePath& cache_dir,
    const web_package::SignedWebBundleId& web_bundle_id,
    const base::Version& version) {
  return GetCacheBundleDirectoryWithVersion(cache_dir, web_bundle_id, version)
      .AppendASCII(kMainSwbnFileName);
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
std::optional<base::FilePath> GetCacheFilePathImpl(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::optional<base::Version>& version,
    const base::FilePath& cache_dir) {
  if (version) {
    base::FilePath expected_file_path =
        GetBundleFullName(cache_dir, web_bundle_id, version.value());
    if (base::PathIsReadable(expected_file_path)) {
      return expected_file_path;
    } else {
      return std::nullopt;
    }
  }
  // When `version` is not provided, take the latest cached version.
  base::FilePath bundle_dir = GetCacheBundleDirectory(cache_dir, web_bundle_id);
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

  return newest_version_path;
}

base::FilePath GetCacheDir(const base::FilePath& base) {
  if (chromeos::IsManagedGuestSession()) {
    return base.AppendASCII(IwaCacheClient::kMgsDirName);
  } else if (chromeos::IsKioskSession()) {
    return base.AppendASCII(IwaCacheClient::kKioskDirName);
  }
  NOTREACHED() << "Unsupported session type for IWA caching";
}

}  // namespace

bool IsIwaBundleCacheEnabled() {
  return base::FeatureList::IsEnabled(features::kIsolatedWebAppBundleCache) &&
         (chromeos::IsManagedGuestSession() || chromeos::IsKioskSession());
}

IwaCacheClient::IwaCacheClient()
    : cache_dir_(GetCacheDir(base::PathService::CheckedGet(
          ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE))) {
  CHECK(IsIwaBundleCacheEnabled())
      << "IwaCacheClient should only be created "
         "inside mgs or kiosk sessions and when the feature is enabled";
}

void IwaCacheClient::GetCacheFilePath(
    const web_package::SignedWebBundleId& web_bundle_id,
    const std::optional<base::Version>& version,
    base::OnceCallback<void(std::optional<base::FilePath>)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetCacheFilePathImpl, web_bundle_id, version, cache_dir_),
      std::move(callback));
}

void IwaCacheClient::SetCacheDirForTesting(const base::FilePath& cache_dir) {
  cache_dir_ = GetCacheDir(cache_dir);
}

}  // namespace web_app
