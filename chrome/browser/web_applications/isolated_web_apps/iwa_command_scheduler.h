// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_IWA_COMMAND_SCHEDULER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_IWA_COMMAND_SCHEDULER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_apply_update_result.h"
#include "chrome/browser/web_applications/model/iwa_update_info.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

class Profile;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;

namespace content {
class StoragePartitionConfig;
}  // namespace content

namespace web_app {

class IsolatedWebAppInstallSource;
class IsolatedWebAppUpdatePrepareAndStoreCommandUpdateInfo;
class IsolatedWebAppUrlInfo;
class SignedWebBundleMetadata;
class WebAppProvider;
enum class IsolatedInstallabilityCheckResult;
struct CleanupOrphanedIsolatedWebAppsCommandError;
struct CleanupOrphanedIsolatedWebAppsCommandSuccess;
struct InstallIsolatedWebAppCommandError;
struct InstallIsolatedWebAppCommandSuccess;
struct IsolatedWebAppUpdatePrepareAndStoreCommandError;
struct IsolatedWebAppUpdatePrepareAndStoreCommandSuccess;

#if BUILDFLAG(IS_CHROMEOS)
class CleanupBundleCacheError;
class CleanupBundleCacheSuccess;
enum class CopyBundleToCacheError;
class CopyBundleToCacheSuccess;
enum class GetBundleCachePathError;
class GetBundleCachePathSuccess;
class RemoveObsoleteBundleVersionsError;
class RemoveObsoleteBundleVersionsSuccess;
#endif  // BUILDFLAG(IS_CHROMEOS)

// Schedules Isolated Web App commands on the `WebAppProvider`'s command
// manager.
class IwaCommandScheduler final {
 public:
  DECLARE_USER_DATA(IwaCommandScheduler);

  using InstallIsolatedWebAppCallback = base::OnceCallback<void(
      base::expected<InstallIsolatedWebAppCommandSuccess,
                     InstallIsolatedWebAppCommandError>)>;
  using CleanupOrphanedIsolatedWebAppsCallback = base::OnceCallback<void(
      base::expected<CleanupOrphanedIsolatedWebAppsCommandSuccess,
                     CleanupOrphanedIsolatedWebAppsCommandError>)>;
  using PrepareAndStoreIsolatedWebAppUpdateCallback = base::OnceCallback<void(
      base::expected<IsolatedWebAppUpdatePrepareAndStoreCommandSuccess,
                     IsolatedWebAppUpdatePrepareAndStoreCommandError>)>;

  static IwaCommandScheduler& From(WebAppProvider& provider);

  IwaCommandScheduler(Profile& profile, ui::UnownedUserDataHost& host);
  IwaCommandScheduler(const IwaCommandScheduler&) = delete;
  IwaCommandScheduler& operator=(const IwaCommandScheduler&) = delete;
  ~IwaCommandScheduler();

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  // Installs an Isolated Web App from the given `url_info` and
  // `install_source`. If `expected_version` is set, this command will refuse to
  // install the Isolated Web App if its version does not match.
  void InstallIsolatedWebApp(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolatedWebAppInstallSource& install_source,
      const std::optional<IwaVersion>& expected_version,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      InstallIsolatedWebAppCallback callback,
      std::optional<IwaUpdateInfo> optional_update_info = std::nullopt,
      const base::Location& call_location = FROM_HERE);

  // Finds and removes any Isolated Web App data directories on disk that are
  // no longer referenced by an installed app in the WebAppRegistrar. This can
  // happen if the browser crashes during IWA installation or uninstallation.
  void CleanupOrphanedIsolatedApps(
      CleanupOrphanedIsolatedWebAppsCallback callback,
      const base::Location& call_location = FROM_HERE);

  // Prepares an update for an Isolated Web App. `update_info` specifies the
  // location of the update for the IWA referred to in `url_info`. This command
  // is safe to run even if the IWA is not installed or already updated. If a
  // dry-run of the update succeeds, then the `update_info` is persisted in the
  // `IsolationData::pending_update_info()` of the IWA in the Web App database.
  void PrepareAndStoreIsolatedWebAppUpdate(
      const IsolatedWebAppUpdatePrepareAndStoreCommandUpdateInfo& update_info,
      const IsolatedWebAppUrlInfo& url_info,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      PrepareAndStoreIsolatedWebAppUpdateCallback callback,
      const base::Location& call_location = FROM_HERE);

  // Applies a prepared pending update to an Isolated Web App. This command is
  // safe to run even if the IWA is not installed or already updated. Regardless
  // of whether the update succeeds or fails,
  // `IsolationData::pending_update_info` of the IWA in the Web App database
  // will be cleared.
  void ApplyPendingIsolatedWebAppUpdate(
      const IsolatedWebAppUrlInfo& url_info,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      base::OnceCallback<void(IsolatedWebAppApplyUpdateCommandResult)> callback,
      const base::Location& call_location = FROM_HERE);

  // Checks if a Signed Web Bundle is a valid and an Isolated Web App that can
  // be installed by the user. It compares the version from the bundle's
  // metadata with an already installed app (if one exists) to determine if the
  // bundle is a new install, an update, or outdated. It also checks if the app
  // is allowlisted to be installed by the user.
  void CheckIsolatedWebAppBundleUserInstallability(
      const SignedWebBundleMetadata& bundle_metadata,
      base::OnceCallback<void(IsolatedInstallabilityCheckResult,
                              std::optional<IwaVersion>)> callback,
      const base::Location& call_location = FROM_HERE);

  // Gets the StoragePartitionConfig for a <controlledframe> within the given
  // Isolated Web App. If the partition is persistent (not `in_memory`), it is
  // registered in the WebAppProvider.
  void GetControlledFramePartition(
      const IsolatedWebAppUrlInfo& url_info,
      const std::string& partition_name,
      bool in_memory,
      base::OnceCallback<void(std::optional<content::StoragePartitionConfig>)>
          callback,
      const base::Location& location = FROM_HERE);

#if BUILDFLAG(IS_CHROMEOS)
  // Gets the path to an IWA bundle in the cache for a given `session_type`. If
  // `version` is not provided, it returns the path to the newest cached
  // version.
  void GetIsolatedWebAppBundleCachePath(
      const IsolatedWebAppUrlInfo& url_info,
      const std::optional<IwaVersion>& version,
      IwaCacheClient::SessionType session_type,
      base::OnceCallback<void(
          base::expected<GetBundleCachePathSuccess, GetBundleCachePathError>)>
          callback,
      const base::Location& call_location = FROM_HERE);

  // Copies an IWA bundle file to the cache for a given `session_type`.
  void CopyIsolatedWebAppBundleToCache(
      const IsolatedWebAppUrlInfo& url_info,
      IwaCacheClient::SessionType session_type,
      base::OnceCallback<void(base::expected<CopyBundleToCacheSuccess,
                                             CopyBundleToCacheError>)> callback,
      const base::Location& call_location = FROM_HERE);

  // Cleans all IWA cached bundles for a given `session_type` that are not in
  // the `iwas_to_keep_in_cache` list.
  void CleanupIsolatedWebAppBundleCache(
      const std::vector<web_package::SignedWebBundleId>& iwas_to_keep_in_cache,
      IwaCacheClient::SessionType session_type,
      base::OnceCallback<void(
          base::expected<CleanupBundleCacheSuccess, CleanupBundleCacheError>)>
          callback,
      const base::Location& call_location = FROM_HERE);

  void RemoveObsoleteIsolatedWebAppVersionsCache(
      const IsolatedWebAppUrlInfo& url_info,
      IwaCacheClient::SessionType session_type,
      base::OnceCallback<
          void(base::expected<RemoveObsoleteBundleVersionsSuccess,
                              RemoveObsoleteBundleVersionsError>)> callback,
      const base::Location& call_location = FROM_HERE);
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  const raw_ref<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;
  ui::ScopedUnownedUserData<IwaCommandScheduler> scoped_data_holder_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_IWA_COMMAND_SCHEDULER_H_
