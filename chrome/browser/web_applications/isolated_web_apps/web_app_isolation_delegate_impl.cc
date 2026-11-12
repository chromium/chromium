// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/web_app_isolation_delegate_impl.h"

#include <memory>
#include <unordered_set>

#include "base/barrier_closure.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/jobs/get_isolated_web_app_size_job.h"
#include "chrome/browser/web_applications/isolated_web_apps/remove_isolated_web_app_data.h"
#include "chrome/browser/web_applications/jobs/compute_app_size_job.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "url/origin.h"

namespace web_app {

// static
std::unique_ptr<WebAppIsolationDelegate> WebAppIsolationDelegateImpl::Create(
    Profile* profile,
    ui::UnownedUserDataHost& unowned_user_data_host) {
  CHECK(AreWebAppsEnabled(profile));
  return base::WrapUnique(
      new WebAppIsolationDelegateImpl(profile, unowned_user_data_host));
}

WebAppIsolationDelegateImpl::~WebAppIsolationDelegateImpl() = default;

void WebAppIsolationDelegateImpl::SetProvider(
    base::PassKey<WebAppProvider> pass_key,
    WebAppProvider& provider) {
  command_scheduler_.SetProvider(pass_key, provider);
  dev_install_manager_.SetProvider(pass_key, provider);
  policy_manager_.SetProvider(pass_key, provider);
  user_installed_manager_.SetProvider(pass_key, provider);
  update_manager_.SetProvider(pass_key, provider);
#if BUILDFLAG(IS_CHROMEOS)
  cache_manager_.SetProvider(pass_key, provider);
#endif
}

void WebAppIsolationDelegateImpl::Start() {
  dev_install_manager_.Start();
  policy_manager_.Start();
  user_installed_manager_.Start();
  update_manager_.Start();
#if BUILDFLAG(IS_CHROMEOS)
  cache_manager_.Start();
#endif
}

void WebAppIsolationDelegateImpl::Shutdown() {
  update_manager_.Shutdown();
}

void WebAppIsolationDelegateImpl::ClearAppResourcesOnUninstall(
    const webapps::AppId& app_id,
    base::OnceClosure callback) {
  // This function is supposed to be called during uninstallation when the app
  // is still in the registrar, yet marked as uninstalling.
  const WebApp* app = WebAppProvider::GetForWebApps(&profile_.get())
                          ->registrar_unsafe()
                          .GetAppById(app_id);
  CHECK(app && app->isolation_data());

  // - Sets pref value to garbage-collect StoragePartitions on next start up.
  // - Clears data on StoragePartitions to prevent data leak on reinstall
  // before GC.
  profile_->GetPrefs()->SetBoolean(
      prefs::kShouldGarbageCollectStoragePartitions, true);

  url::Origin iwa_origin = url::Origin::Create(app->scope());
  auto location = app->isolation_data()->location();

  auto barrier_closure = base::BarrierClosure(2, std::move(callback));

  RemoveIsolatedWebAppBrowsingData(&profile_.get(), iwa_origin,
                                   barrier_closure);
  CloseAndDeleteBundle(&profile_.get(), location, barrier_closure);
}

std::unique_ptr<ComputeAppSizeJob>
WebAppIsolationDelegateImpl::CreateComputeAppSizeJob(
    const webapps::AppId& app_id,
    base::DictValue& debug_value) {
  return std::make_unique<GetIsolatedWebAppSizeJob>(&profile_.get(), app_id,
                                                    debug_value);
}

std::unordered_set<base::FilePath>
WebAppIsolationDelegateImpl::GetIsolatedStoragePaths() {
  std::unordered_set<base::FilePath> paths;
  for (const auto& app : WebAppProvider::GetForWebApps(&profile_.get())
                             ->registrar_unsafe()
                             .GetApps(WebAppFilter::IsIsolatedApp())) {
    auto url_info = IsolatedWebAppUrlInfo::Create(app.scope());
    if (url_info.has_value()) {
      paths.insert(profile_
                       ->GetStoragePartition(
                           url_info->storage_partition_config(&profile_.get()))
                       ->GetPath());
    }
  }
  return paths;
}

WebAppIsolationDelegateImpl::WebAppIsolationDelegateImpl(
    Profile* profile,
    ui::UnownedUserDataHost& unowned_user_data_host)
    : profile_(*profile),
      command_scheduler_(*profile_, unowned_user_data_host),
      dev_install_manager_(*profile_, unowned_user_data_host),
      policy_manager_(&*profile_, unowned_user_data_host),
      user_installed_manager_(unowned_user_data_host),
      update_manager_(*profile_, unowned_user_data_host)
#if BUILDFLAG(IS_CHROMEOS)
      ,
      cache_manager_(*profile_, unowned_user_data_host)
#endif
{
}

}  // namespace web_app
