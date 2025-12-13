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
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"

namespace web_app {

using SessionType = IwaCacheClient::SessionType;

bool IsIwaBundleCacheEnabledInCurrentSession() {
  return IsIwaBundleCacheFeatureEnabled() &&
         (chromeos::IsManagedGuestSession() || chromeos::IsIwaKioskSession());
}

bool IsIwaBundleCacheFeatureEnabled() {
  return base::FeatureList::IsEnabled(features::kIsolatedWebAppBundleCache);
}

// static
SessionType IwaCacheClient::GetCurrentSessionType() {
  if (chromeos::IsKioskSession()) {
    return SessionType::kKiosk;
  }
  if (chromeos::IsManagedGuestSession()) {
    return SessionType::kManagedGuestSession;
  }
  NOTREACHED()
      << "IwaCacheClient supports only kiosk and Managed Guest Session.";
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
base::FilePath IwaCacheClient::GetCacheDirectoryForBundleWithVersion(
    const base::FilePath& cache_base_dir,
    const web_package::SignedWebBundleId& web_bundle_id,
    const IwaVersion& version) {
  return IwaCacheClient::GetCacheDirectoryForBundle(cache_base_dir,
                                                    web_bundle_id)
      .AppendASCII(version.GetString());
}

// static
base::FilePath IwaCacheClient::GetBundleFullName(
    const base::FilePath& bundle_dir_with_version) {
  return bundle_dir_with_version.AppendASCII(kMainSwbnFileName);
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
