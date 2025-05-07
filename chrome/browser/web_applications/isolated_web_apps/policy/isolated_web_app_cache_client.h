// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_CACHE_CLIENT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_CACHE_CLIENT_H_

#include <optional>

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/path_service.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_downloader.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app {

// Cache is enabled only for Managed Guest Session (MGS) and for kiosk sessions
// and only when the feature flag is enabled.
bool IsIwaBundleCacheEnabled();

// This class should be used only when `IsIwaBundleCacheEnabled()` returns
// true. This is checked in the constructor. This class can be created
// multiple times even for the same IWA.
class IwaCacheClient {
 public:
  enum class SessionType {
    kKiosk,
    kManagedGuestSession,
  };

  static SessionType GetCurrentSessionType();

  struct CachedBundleData {
    base::FilePath path;
    base::Version version;
  };

  IwaCacheClient();
  IwaCacheClient(const IwaCacheClient&) = delete;
  IwaCacheClient& operator=(const IwaCacheClient&) = delete;
  ~IwaCacheClient() = default;

  // Calls `callback` with the path of the cached bundle and it's version.
  // If the IWA is not cached, returns `std::nullopt`.
  // `version` may be empty, which means the function returns the bundle path
  // with the newest cached version.
  // If `version` is provided, return the bundle with specified version. If this
  // version is not cached, returns `std::nullopt`.
  void GetCacheFilePath(
      const web_package::SignedWebBundleId& web_bundle_id,
      const std::optional<base::Version>& version,
      base::OnceCallback<void(std::optional<CachedBundleData>)> callback);

  // TODO(crbug.com/392069400): clean cache for old IWA versions.

  void SetCacheDirForTesting(const base::FilePath& cache_dir);

  static base::FilePath GetCacheBaseDirectoryForSessionType(
      IwaCacheClient::SessionType session_type,
      const base::FilePath& base = base::PathService::CheckedGet(
          ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE));

  static base::FilePath GetCacheDirectoryForBundle(
      const base::FilePath& cache_base_dir,
      const web_package::SignedWebBundleId& web_bundle_id);

  static base::FilePath GetCacheDirectoryForBundleWithVersion(
      const base::FilePath& cache_dir,
      const web_package::SignedWebBundleId& web_bundle_id,
      const base::Version& version);

  static base::FilePath GetBundleFullName(
      const base::FilePath& bundle_dir_with_version);

  static std::string SessionTypeToString(SessionType session_type);

  static constexpr base::FilePath::CharType kMgsDirName[] = "mgs";
  static constexpr base::FilePath::CharType kKioskDirName[] = "kiosk";

 private:
  base::FilePath cache_dir_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_CACHE_CLIENT_H_
