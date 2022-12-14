// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_manager.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/install_result_code.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/containers/cxx20_erase.h"
#endif

namespace web_app {

ExternallyManagedAppManager::InstallResult::InstallResult() = default;

ExternallyManagedAppManager::InstallResult::InstallResult(
    webapps::InstallResultCode code,
    absl::optional<AppId> app_id,
    bool did_uninstall_and_replace)
    : code(code),
      app_id(std::move(app_id)),
      did_uninstall_and_replace(did_uninstall_and_replace) {}

ExternallyManagedAppManager::InstallResult::InstallResult(
    const InstallResult&) = default;

ExternallyManagedAppManager::InstallResult::~InstallResult() = default;

bool ExternallyManagedAppManager::InstallResult::operator==(
    const InstallResult& other) const {
  return std::tie(code, app_id, did_uninstall_and_replace) ==
         std::tie(other.code, other.app_id, other.did_uninstall_and_replace);
}

ExternallyManagedAppManager::SynchronizeRequest::SynchronizeRequest(
    SynchronizeCallback callback,
    std::vector<ExternalInstallOptions> pending_installs,
    int remaining_uninstall_requests)
    : callback(std::move(callback)),
      remaining_install_requests(pending_installs.size()),
      pending_installs(std::move(pending_installs)),
      remaining_uninstall_requests(remaining_uninstall_requests) {}

ExternallyManagedAppManager::SynchronizeRequest::~SynchronizeRequest() =
    default;

ExternallyManagedAppManager::SynchronizeRequest&
ExternallyManagedAppManager::SynchronizeRequest::operator=(
    ExternallyManagedAppManager::SynchronizeRequest&&) = default;

ExternallyManagedAppManager::SynchronizeRequest::SynchronizeRequest(
    SynchronizeRequest&& other) = default;

ExternallyManagedAppManager::ExternallyManagedAppManager() = default;

ExternallyManagedAppManager::~ExternallyManagedAppManager() {
  DCHECK(!registration_callback_);
}

void ExternallyManagedAppManager::SetSubsystems(
    WebAppRegistrar* registrar,
    WebAppUiManager* ui_manager,
    WebAppInstallFinalizer* finalizer,
    WebAppCommandScheduler* command_scheduler,
    WebAppSyncBridge* sync_bridge) {
  registrar_ = registrar;
  ui_manager_ = ui_manager;
  finalizer_ = finalizer;
  command_scheduler_ = command_scheduler;
  sync_bridge_ = sync_bridge;
}

void ExternallyManagedAppManager::SynchronizeInstalledApps(
    std::vector<ExternalInstallOptions> desired_apps_install_options,
    ExternalInstallSource install_source,
    SynchronizeCallback callback) {
  DCHECK(registrar_);
  DCHECK(base::ranges::all_of(
      desired_apps_install_options,
      [&install_source](const ExternalInstallOptions& install_options) {
        return install_options.install_source == install_source;
      }));
  // Only one concurrent SynchronizeInstalledApps() expected per
  // ExternalInstallSource.
  DCHECK(!base::Contains(synchronize_requests_, install_source));

  std::vector<GURL> installed_urls;
  for (const auto& apps_it :
       registrar_->GetExternallyInstalledApps(install_source)) {
    // TODO(crbug.com/1339965): Remove this check once we cleanup
    // ExternallyInstalledWebAppPrefs on external app uninstall.
    bool has_same_external_source =
        registrar_->GetAppById(apps_it.first)
            ->GetSources()
            .test(ConvertExternalInstallSourceToSource(install_source));
    if (has_same_external_source) {
      for (const GURL& url : apps_it.second) {
        installed_urls.push_back(url);
      }
    }
  }

  std::sort(installed_urls.begin(), installed_urls.end());

  std::vector<GURL> desired_urls;
  desired_urls.reserve(desired_apps_install_options.size());
  for (const auto& info : desired_apps_install_options)
    desired_urls.push_back(info.install_url);

  std::sort(desired_urls.begin(), desired_urls.end());

  std::vector<GURL> urls_to_remove =
      base::STLSetDifference<std::vector<GURL>>(installed_urls, desired_urls);

#if BUILDFLAG(IS_CHROMEOS)
  // This check ensures that on Chrome OS, the messages app is not uninstalled
  // automatically when SynchronizeInstalledApps() is called for preinstalled
  // apps.
  // TODO(crbug.com/1239801): Once Messages has been migrated to be a
  // preinstalled app, this logic can be removed because the
  // PreInstalledWebAppManager will take care of this.
  if (!urls_to_remove.empty() &&
      ConvertExternalInstallSourceToSource(install_source) ==
          WebAppManagement::kDefault) {
    base::EraseIf(urls_to_remove, [&](const GURL& url) {
      return url.spec() ==
                 "https://messages-web.sandbox.google.com/web/authentication" ||
             url.spec() == "https://messages.google.com/web/authentication";
    });
  }
#endif

  // Run callback immediately if there's no work to be done.
  if (urls_to_remove.empty() && desired_apps_install_options.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::map<GURL, InstallResult>(),
                       std::map<GURL, bool>()));
    return;
  }

  // Add the callback to a map and call once all installs/uninstalls finish.
  synchronize_requests_.insert_or_assign(
      install_source,
      SynchronizeRequest(std::move(callback),
                         std::move(desired_apps_install_options),
                         urls_to_remove.size()));

  if (urls_to_remove.empty()) {
    // If there are no uninstalls, this will trigger the installs.
    ContinueOrCompleteSynchronization(install_source);
  } else {
    UninstallApps(
        urls_to_remove, install_source,
        base::BindRepeating(
            &ExternallyManagedAppManager::UninstallForSynchronizeCallback,
            weak_ptr_factory_.GetWeakPtr(), install_source));
  }
}

void ExternallyManagedAppManager::SetRegistrationCallbackForTesting(
    RegistrationCallback callback) {
  registration_callback_ = std::move(callback);
}

void ExternallyManagedAppManager::ClearRegistrationCallbackForTesting() {
  registration_callback_ = RegistrationCallback();
}

void ExternallyManagedAppManager::SetRegistrationsCompleteCallbackForTesting(
    base::OnceClosure callback) {
  registrations_complete_callback_ = std::move(callback);
}

void ExternallyManagedAppManager::OnRegistrationFinished(
    const GURL& install_url,
    RegistrationResultCode result) {
  if (registration_callback_)
    registration_callback_.Run(install_url, result);
}

void ExternallyManagedAppManager::InstallForSynchronizeCallback(
    ExternalInstallSource source,
    const GURL& install_url,
    ExternallyManagedAppManager::InstallResult result) {
  if (!IsSuccess(result.code)) {
    LOG(ERROR) << install_url << " from install source "
               << static_cast<int>(source) << " failed to install with reason "
               << static_cast<int>(result.code);
  }

  auto source_and_request = synchronize_requests_.find(source);
  DCHECK(source_and_request != synchronize_requests_.end());
  SynchronizeRequest& request = source_and_request->second;
  request.install_results[install_url] = std::move(result);
  --request.remaining_install_requests;
  DCHECK_GE(request.remaining_install_requests, 0);

  ContinueOrCompleteSynchronization(source);
}

void ExternallyManagedAppManager::UninstallForSynchronizeCallback(
    ExternalInstallSource source,
    const GURL& install_url,
    bool succeeded) {
  auto source_and_request = synchronize_requests_.find(source);
  DCHECK(source_and_request != synchronize_requests_.end());
  SynchronizeRequest& request = source_and_request->second;
  request.uninstall_results[install_url] = succeeded;
  --request.remaining_uninstall_requests;
  DCHECK_GE(request.remaining_uninstall_requests, 0);

  ContinueOrCompleteSynchronization(source);
}

void ExternallyManagedAppManager::ContinueOrCompleteSynchronization(
    ExternalInstallSource source) {
  auto source_and_request = synchronize_requests_.find(source);
  DCHECK(source_and_request != synchronize_requests_.end());

  SynchronizeRequest& request = source_and_request->second;

  if (request.remaining_uninstall_requests > 0)
    return;

  // Installs only take place after all uninstalls.
  if (!request.pending_installs.empty()) {
    DCHECK_GT(request.remaining_install_requests, 0);
    // Note: It is intentional that std::move(request.pending_installs) clears
    // the vector in `request`, preventing this branch from triggering again.
    InstallApps(std::move(request.pending_installs),
                base::BindRepeating(
                    &ExternallyManagedAppManager::InstallForSynchronizeCallback,
                    weak_ptr_factory_.GetWeakPtr(), source));
    return;
  }

  if (request.remaining_install_requests > 0)
    return;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(request.callback),
                                std::move(request.install_results),
                                std::move(request.uninstall_results)));
  synchronize_requests_.erase(source);
}
void ExternallyManagedAppManager::ClearSynchronizeRequestsForTesting() {
  synchronize_requests_.erase(synchronize_requests_.begin(),
                              synchronize_requests_.end());
}

}  // namespace web_app
