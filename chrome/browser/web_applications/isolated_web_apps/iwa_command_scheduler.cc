// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/iwa_command_scheduler.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/check_isolated_web_app_bundle_user_installability_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/cleanup_orphaned_isolated_web_apps_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/get_controlled_frame_partition_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_prepare_and_store_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/storage_partition_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/isolated_web_apps/commands/cleanup_bundle_cache_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/copy_bundle_to_cache_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/get_bundle_cache_path_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/remove_obsolete_bundle_versions_cache_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

DEFINE_USER_DATA(IwaCommandScheduler);

// static
IwaCommandScheduler& IwaCommandScheduler::From(WebAppProvider& provider) {
  return CHECK_DEREF(Get(provider.unowned_user_data_host()));
}

IwaCommandScheduler::IwaCommandScheduler(Profile& profile,
                                         ui::UnownedUserDataHost& host)
    : profile_(profile), scoped_data_holder_(host, *this) {}

IwaCommandScheduler::~IwaCommandScheduler() = default;

void IwaCommandScheduler::SetProvider(base::PassKey<WebAppProvider>,
                                      WebAppProvider& provider) {
  provider_ = &provider;
}

void IwaCommandScheduler::InstallIsolatedWebApp(
    const IsolatedWebAppUrlInfo& url_info,
    const IsolatedWebAppInstallSource& install_source,
    const std::optional<IwaVersion>& expected_version,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    InstallIsolatedWebAppCallback callback,
    std::optional<IwaUpdateInfo> optional_update_info,
    const base::Location& call_location) {
  CHECK(optional_profile_keep_alive == nullptr ||
        optional_profile_keep_alive->profile() == &*profile_);
  provider_->command_manager().ScheduleCommand(
      std::make_unique<InstallIsolatedWebAppCommand>(
          url_info, install_source, expected_version, *profile_,
          std::move(optional_keep_alive),
          std::move(optional_profile_keep_alive), std::move(callback),
          std::move(optional_update_info)),
      call_location);
}

void IwaCommandScheduler::CleanupOrphanedIsolatedApps(
    CleanupOrphanedIsolatedWebAppsCallback callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<CleanupOrphanedIsolatedWebAppsCommand>(
          *profile_, std::move(callback)),
      call_location);
}

void IwaCommandScheduler::PrepareAndStoreIsolatedWebAppUpdate(
    const IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo& update_info,
    const IsolatedWebAppUrlInfo& url_info,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    PrepareAndStoreIsolatedWebAppUpdateCallback callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<IsolatedWebAppUpdatePrepareAndStoreCommand>(
          update_info, url_info, *profile_, std::move(optional_keep_alive),
          std::move(optional_profile_keep_alive), std::move(callback)),
      call_location);
}

void IwaCommandScheduler::ApplyPendingIsolatedWebAppUpdate(
    const IsolatedWebAppUrlInfo& url_info,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    base::OnceCallback<void(IsolatedWebAppApplyUpdateCommandResult)> callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<IsolatedWebAppApplyUpdateCommand>(
          url_info, *profile_, std::move(optional_keep_alive),
          std::move(optional_profile_keep_alive), std::move(callback)),
      call_location);
}

void IwaCommandScheduler::CheckIsolatedWebAppBundleUserInstallability(
    const SignedWebBundleMetadata& bundle_metadata,
    base::OnceCallback<void(IsolatedInstallabilityCheckResult,
                            std::optional<IwaVersion>)> callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<CheckIsolatedWebAppBundleUserInstallabilityCommand>(
          &profile_.get(), bundle_metadata, std::move(callback)),
      call_location);
}

void IwaCommandScheduler::GetControlledFramePartition(
    const IsolatedWebAppUrlInfo& url_info,
    const std::string& partition_name,
    bool in_memory,
    base::OnceCallback<void(std::optional<content::StoragePartitionConfig>)>
        callback,
    const base::Location& location) {
  provider_->scheduler().ScheduleCallbackWithResult(
      "GetControlledFramePartition", AppLockDescription(url_info.app_id()),
      base::BindOnce(&GetControlledFramePartitionWithLock, &profile_.get(),
                     url_info, partition_name, in_memory),
      std::move(callback),
      /*arg_for_shutdown=*/
      std::optional<content::StoragePartitionConfig>(std::nullopt), location);
}

#if BUILDFLAG(IS_CHROMEOS)
void IwaCommandScheduler::GetIsolatedWebAppBundleCachePath(
    const IsolatedWebAppUrlInfo& url_info,
    const std::optional<IwaVersion>& version,
    IwaCacheClient::SessionType session_type,
    base::OnceCallback<void(base::expected<GetBundleCachePathSuccess,
                                           GetBundleCachePathError>)> callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<GetBundleCachePathCommand>(
          url_info, version, session_type, std::move(callback)),
      call_location);
}

void IwaCommandScheduler::CopyIsolatedWebAppBundleToCache(
    const IsolatedWebAppUrlInfo& url_info,
    IwaCacheClient::SessionType session_type,
    base::OnceCallback<void(CopyBundleToCacheResult)> callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<CopyBundleToCacheCommand>(
          url_info, session_type, *profile_, std::move(callback)),
      call_location);
}

void IwaCommandScheduler::CleanupIsolatedWebAppBundleCache(
    const std::vector<web_package::SignedWebBundleId>& iwas_to_keep_in_cache,
    IwaCacheClient::SessionType session_type,
    base::OnceCallback<void(CleanupBundleCacheResult)> callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<CleanupBundleCacheCommand>(
          iwas_to_keep_in_cache, session_type, std::move(callback)),
      call_location);
}

void IwaCommandScheduler::RemoveObsoleteIsolatedWebAppVersionsCache(
    const IsolatedWebAppUrlInfo& url_info,
    IwaCacheClient::SessionType session_type,
    base::OnceCallback<void(RemoveObsoleteBundleVersionsResult)> callback,
    const base::Location& call_location) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<RemoveObsoleteBundleVersionsCacheCommand>(
          url_info, session_type, std::move(callback)),
      call_location);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
