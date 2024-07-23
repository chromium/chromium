// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_manager.h"

#include <map>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_install_task.h"
#include "chrome/browser/web_applications/externally_managed_app_registration_task.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

namespace web_app {

ExternallyManagedAppManager::InstallResult::InstallResult() = default;

ExternallyManagedAppManager::InstallResult::InstallResult(
    webapps::InstallResultCode code,
    std::optional<webapps::AppId> app_id,
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
      remaining_uninstall_requests(remaining_uninstall_requests) {
  CHECK(this->callback);
}

ExternallyManagedAppManager::SynchronizeRequest::~SynchronizeRequest() =
    default;

ExternallyManagedAppManager::SynchronizeRequest&
ExternallyManagedAppManager::SynchronizeRequest::operator=(
    ExternallyManagedAppManager::SynchronizeRequest&&) = default;

ExternallyManagedAppManager::SynchronizeRequest::SynchronizeRequest(
    SynchronizeRequest&& other) = default;

struct ExternallyManagedAppManager::TaskAndCallback {
  TaskAndCallback(std::unique_ptr<ExternallyManagedAppInstallTask> task,
                  OnceInstallCallback callback)
      : task(std::move(task)), callback(std::move(callback)) {}
  ~TaskAndCallback() = default;

  std::unique_ptr<ExternallyManagedAppInstallTask> task;
  OnceInstallCallback callback;
};

ExternallyManagedAppManager::ExternallyManagedAppManager(Profile* profile)
    : profile_(profile) {}

ExternallyManagedAppManager::~ExternallyManagedAppManager() {
  DCHECK(!registration_callback_);
  // Extra check to verify that web_contents is released even if
  // shutdown somehow has not been invoked.
  if (!IsShuttingDown()) {
    Shutdown();
  }
}

void ExternallyManagedAppManager::SetProvider(base::PassKey<WebAppProvider>,
                                              WebAppProvider& provider) {
  provider_ = &provider;
  // TODO(http://b/283521737): Remove this and use WebContentsManager.
  url_loader_ = provider.web_contents_manager().CreateUrlLoader();
  // TODO(http://b/283521737): Remove this and use WebContentsManager.
  data_retriever_factory_ = base::BindRepeating(
      [](base::WeakPtr<WebContentsManager> web_contents_manager)
          -> std::unique_ptr<WebAppDataRetriever> {
        if (!web_contents_manager) {
          return nullptr;
        }
        return web_contents_manager->CreateDataRetriever();
      },
      provider.web_contents_manager().GetWeakPtr());
}

void ExternallyManagedAppManager::InstallNow(
    ExternalInstallOptions install_options,
    OnceInstallCallback callback) {
  pending_installs_.push_front(std::make_unique<TaskAndCallback>(
      CreateInstallationTask(std::move(install_options)), std::move(callback)));

  PostMaybeStartNext();
}

void ExternallyManagedAppManager::Install(
    ExternalInstallOptions install_options,
    OnceInstallCallback callback) {
  pending_installs_.push_back(std::make_unique<TaskAndCallback>(
      CreateInstallationTask(std::move(install_options)), std::move(callback)));

  PostMaybeStartNext();
}

void ExternallyManagedAppManager::InstallApps(
    std::vector<ExternalInstallOptions> install_options_list,
    const RepeatingInstallCallback& callback) {
  for (auto& install_options : install_options_list) {
    pending_installs_.push_back(std::make_unique<TaskAndCallback>(
        CreateInstallationTask(std::move(install_options)), callback));
  }

  PostMaybeStartNext();
}

void ExternallyManagedAppManager::UninstallApps(
    std::vector<GURL> uninstall_urls,
    ExternalInstallSource install_source,
    const UninstallCallback& callback) {
  for (auto& url : uninstall_urls) {
    provider_->scheduler().RemoveInstallUrlMaybeUninstall(
        /*app_id=*/std::nullopt,
        ConvertExternalInstallSourceToSource(install_source), url,
        ConvertExternalInstallSourceToUninstallSource(install_source),
        base::BindOnce(
            [](const UninstallCallback& callback, const GURL& app_url,
               webapps::UninstallResultCode code) {
              callback.Run(app_url, code);
            },
            callback, url));
  }
}

void ExternallyManagedAppManager::SynchronizeInstalledApps(
    std::vector<ExternalInstallOptions> desired_apps_install_options,
    ExternalInstallSource install_source,
    SynchronizeCallback callback) {
  CHECK(callback);
  CHECK(base::ranges::all_of(
      desired_apps_install_options,
      [&install_source](const ExternalInstallOptions& install_options) {
        return install_options.install_source == install_source;
      }));
  // Only one concurrent SynchronizeInstalledApps() expected per
  // ExternalInstallSource.
  CHECK(!base::Contains(synchronize_requests_, install_source));
  provider_->scheduler().ScheduleCallback<AllAppsLock>(
      "ExternallyManagedAppManager::SynchronizeInstalledApps",
      AllAppsLockDescription(),
      base::BindOnce(
          &ExternallyManagedAppManager::SynchronizeInstalledAppsOnLockAcquired,
          weak_ptr_factory_.GetWeakPtr(),
          std::move(desired_apps_install_options), install_source,
          std::move(callback)),
      /*on_complete=*/base::DoNothing());
}

void ExternallyManagedAppManager::Shutdown() {
  is_in_shutdown_ = true;
  pending_registrations_.clear();
  current_registration_.reset();
  pending_installs_.clear();
  url_loader_.reset();
  // `current_install_` keeps a pointer to `web_contents_` so destroy it before
  // releasing the WebContents.
  current_install_.reset();
  ReleaseWebContents();
}

void ExternallyManagedAppManager::SetUrlLoaderForTesting(
    std::unique_ptr<webapps::WebAppUrlLoader> url_loader) {
  CHECK_IS_TEST();
  url_loader_ = std::move(url_loader);
}

void ExternallyManagedAppManager::SetDataRetrieverFactoryForTesting(
    base::RepeatingCallback<std::unique_ptr<WebAppDataRetriever>()> factory) {
  CHECK_IS_TEST();
  data_retriever_factory_ = std::move(factory);
}

void ExternallyManagedAppManager::ReleaseWebContents() {
  DCHECK(pending_registrations_.empty());
  DCHECK(!current_registration_);
  DCHECK(pending_installs_.empty());
  DCHECK(!current_install_);

  web_contents_.reset();
}

std::unique_ptr<ExternallyManagedAppInstallTask>
ExternallyManagedAppManager::CreateInstallationTask(
    ExternalInstallOptions install_options) {
  std::unique_ptr<ExternallyManagedAppInstallTask> install_task =
      std::make_unique<ExternallyManagedAppInstallTask>(
          *provider_, std::move(install_options));
  return install_task;
}

std::unique_ptr<ExternallyManagedAppRegistrationTaskBase>
ExternallyManagedAppManager::CreateRegistration(
    GURL install_url,
    const base::TimeDelta registration_timeout) {
  DCHECK(!IsShuttingDown());
  ExternallyManagedAppRegistrationTask::RegistrationCallback callback =
      base::BindOnce(&ExternallyManagedAppManager::OnRegistrationFinished,
                     weak_ptr_factory_.GetWeakPtr(), install_url);
  return std::make_unique<ExternallyManagedAppRegistrationTask>(
      std::move(install_url), registration_timeout, url_loader_.get(),
      web_contents_.get(), std::move(callback));
}

void ExternallyManagedAppManager::OnRegistrationFinished(
    const GURL& install_url,
    RegistrationResultCode result) {
  if (IsShuttingDown()) {
    return;
  }

  DCHECK(!current_registration_ ||
         current_registration_->install_url() == install_url);

  if (registration_callback_) {
    registration_callback_.Run(install_url, result);
  }

  current_registration_.reset();
  PostMaybeStartNext();
}

void ExternallyManagedAppManager::PostMaybeStartNext() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ExternallyManagedAppManager::MaybeStartNext,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ExternallyManagedAppManager::MaybeStartNext() {
  if (current_install_ || IsShuttingDown()) {
    return;
  }
  provider_->scheduler().ScheduleCallback(
      "ExternallyManagedAppManager::MaybeStartNext", AllAppsLockDescription(),
      base::BindOnce(&ExternallyManagedAppManager::MaybeStartNextOnLockAcquired,
                     weak_ptr_factory_.GetWeakPtr()),
      /*on_complete=*/base::DoNothing());
}

void ExternallyManagedAppManager::MaybeStartNextOnLockAcquired(
    AllAppsLock& lock,
    base::Value::Dict& debug_value) {
  if (current_install_ || IsShuttingDown()) {
    return;
  }

  while (!pending_installs_.empty()) {
    std::unique_ptr<TaskAndCallback> front =
        std::move(pending_installs_.front());
    pending_installs_.pop_front();

    const ExternalInstallOptions& install_options =
        front->task->install_options();

    CHECK(install_options.install_url.is_valid(), base::NotFatalUntil::M130);
    std::optional<webapps::AppId> app_id =
        lock.registrar().LookupExternalAppId(install_options.install_url);
    debug_value.Set("app_id_from_install_url", app_id.value_or("<none>"));

    const bool is_placeholder_installed =
        app_id.has_value()
            ? lock.registrar().IsPlaceholderApp(
                  app_id.value(), ConvertExternalInstallSourceToSource(
                                      install_options.install_source))
            : false;
    debug_value.Set("is_placeholder_installed", is_placeholder_installed);

    if (install_options.force_reinstall) {
      StartInstallationTask(
          std::move(front),
          /*installed_placeholder_app_id=*/is_placeholder_installed
              ? std::move(app_id)
              : std::nullopt);
      return;
    }

    // If the URL is not in web_app registrar,
    // then no external source has installed it.
    if (!app_id.has_value()) {
      StartInstallationTask(std::move(front),
                            /*installed_placeholder_app_id=*/std::nullopt);
      return;
    }

    if (lock.registrar().IsInstalled(app_id.value())) {
      if (install_options.placeholder_resolution_behavior ==
              PlaceholderResolutionBehavior::kWaitForAppWindowsClosed &&
          lock.ui_manager().GetNumWindowsForApp(app_id.value()) != 0) {
        debug_value.Set("waiting_for_windows_closed", true);
        lock.ui_manager().NotifyOnAllAppWindowsClosed(
            app_id.value(),
            base::BindOnce(&ExternallyManagedAppManager::Install,
                           weak_ptr_factory_.GetWeakPtr(), install_options,
                           std::move(front->callback)));
        continue;
      }

      // If the app is already installed, only reinstall it if the app is a
      // placeholder app and the client asked for it to be reinstalled.
      if (is_placeholder_installed) {
        debug_value.Set("retrying_install_because_placeholder", true);
        StartInstallationTask(std::move(front), std::move(app_id));
        return;
      }

      // TODO(crbug.com/40216215): Investigate re-install of the app for all
      // WebAppManagement sources.
      if ((ConvertExternalInstallSourceToSource(
               install_options.install_source) == WebAppManagement::kPolicy) &&
          (!lock.registrar()
                .GetAppById(app_id.value())
                ->IsPolicyInstalledApp())) {
        debug_value.Set("reinstalling_policy_app", true);
        StartInstallationTask(std::move(front),
                              /*installed_placeholder_app_id=*/std::nullopt);
        return;
      } else {
        debug_value.Set("simple_source_addition", true);
        // Add install source before returning the result.
        ScopedRegistryUpdate update = lock.sync_bridge().BeginUpdate();
        WebApp* app_to_update = update->UpdateApp(app_id.value());
        app_to_update->AddSource(ConvertExternalInstallSourceToSource(
            install_options.install_source));
        app_to_update->AddInstallURLToManagementExternalConfigMap(
            ConvertExternalInstallSourceToSource(
                install_options.install_source),
            install_options.install_url);
      }
      std::move(front->callback)
          .Run(install_options.install_url,
               ExternallyManagedAppManager::InstallResult(
                   webapps::InstallResultCode::kSuccessAlreadyInstalled,
                   app_id));
      continue;
    }

    // If neither of the above conditions applies, the app probably got
    // uninstalled but it wasn't been removed from the map. We should install
    // the app in this case.
    StartInstallationTask(std::move(front),
                          /*installed_placeholder_app_id=*/std::nullopt);
    return;
  }
  DCHECK(!current_install_);

  if (current_registration_ || RunNextRegistration()) {
    return;
  }

  ReleaseWebContents();
}

void ExternallyManagedAppManager::StartInstallationTask(
    std::unique_ptr<TaskAndCallback> task,
    std::optional<webapps::AppId> installed_placeholder_app_id) {
  if (IsShuttingDown()) {
    return;
  }
  DCHECK(!current_install_);
  DCHECK(!is_in_shutdown_);
  if (current_registration_) {
    // Preempt current registration.
    pending_registrations_.push_front(
        {current_registration_->install_url(),
         current_registration_->registration_timeout()});
    current_registration_.reset();
  }

  current_install_ = std::move(task);
  CreateWebContentsIfNecessary();
  current_install_->task->Install(
      std::move(installed_placeholder_app_id),
      base::BindOnce(&ExternallyManagedAppManager::OnInstalled,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool ExternallyManagedAppManager::RunNextRegistration() {
  if (pending_registrations_.empty() || IsShuttingDown()) {
    if (registrations_complete_callback_) {
      std::move(registrations_complete_callback_).Run();
    }
    return false;
  }

  const auto [url_to_check, registration_timeout] =
      std::move(pending_registrations_.front());
  pending_registrations_.pop_front();
  current_registration_ =
      CreateRegistration(std::move(url_to_check), registration_timeout);
  current_registration_->Start();
  return true;
}

void ExternallyManagedAppManager::CreateWebContentsIfNecessary() {
  DCHECK(!IsShuttingDown());
  if (web_contents_) {
    return;
  }

  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  CreateWebAppInstallTabHelpers(web_contents_.get());
}

void ExternallyManagedAppManager::OnInstalled(
    ExternallyManagedAppManager::InstallResult result) {
  if (result.app_id && IsSuccess(result.code)) {
    MaybeEnqueueServiceWorkerRegistration(
        current_install_->task->install_options());
  }

  // Post a task to avoid webapps::InstallableManager crashing and do so before
  // running the callback in case the callback tries to install another
  // app.
  PostMaybeStartNext();

  std::unique_ptr<TaskAndCallback> task_and_callback;
  task_and_callback.swap(current_install_);
  std::move(task_and_callback->callback)
      .Run(task_and_callback->task->install_options().install_url, result);
}

void ExternallyManagedAppManager::MaybeEnqueueServiceWorkerRegistration(
    const ExternalInstallOptions& install_options) {
  if (IsShuttingDown()) {
    return;
  }

  if (install_options.only_use_app_info_factory) {
    return;
  }

  if (!install_options.load_and_await_service_worker_registration) {
    return;
  }

  // TODO(crbug.com/40561535): Call CreateWebContentsIfNecessary() instead of
  // checking web_contents_ once major migration of default hosted apps to web
  // apps has completed.
  // Temporarily using offline manifest migrations (in which |web_contents_|
  // is nullptr) in order to avoid overwhelming migrated-to web apps with hits
  // for service worker registrations.
  if (!web_contents_) {
    return;
  }

  GURL url = install_options.service_worker_registration_url.value_or(
      install_options.install_url);
  if (url.is_empty()) {
    return;
  }
  if (url.scheme() == content::kChromeUIScheme) {
    return;
  }
  if (url.scheme() == content::kChromeUIUntrustedScheme) {
    return;
  }

  pending_registrations_.push_back(
      {url, install_options.service_worker_registration_timeout});
}

bool ExternallyManagedAppManager::IsShuttingDown() {
  return is_in_shutdown_ || profile()->ShutdownStarted();
}

void ExternallyManagedAppManager::SynchronizeInstalledAppsOnLockAcquired(
    std::vector<ExternalInstallOptions> desired_apps_install_options,
    ExternalInstallSource install_source,
    SynchronizeCallback callback,
    AllAppsLock& lock,
    base::Value::Dict& debug_info) {
  CHECK(callback);
  debug_info.Set("install_source", base::ToString(install_source));
  base::Value::List* desired_installs =
      debug_info.EnsureList("desired_apps_install_options");
  for (const ExternalInstallOptions& option : desired_apps_install_options) {
    desired_installs->Append(option.install_url.spec());
  }
  std::vector<GURL> installed_urls;
  for (const auto& [app_id, install_urls] :
       lock.registrar().GetExternallyInstalledApps(install_source)) {
    for (const GURL& url : install_urls) {
      installed_urls.push_back(url);
    }
  }

  std::sort(installed_urls.begin(), installed_urls.end());

  base::Value::List* desired_urls_debug = debug_info.EnsureList("desired_urls");
  std::vector<GURL> desired_urls;
  desired_urls.reserve(desired_apps_install_options.size());
  for (const auto& info : desired_apps_install_options) {
    desired_urls.push_back(info.install_url);
    desired_urls_debug->Append(info.install_url.spec());
  }

  std::sort(desired_urls.begin(), desired_urls.end());

  std::vector<GURL> urls_to_remove =
      base::STLSetDifference<std::vector<GURL>>(installed_urls, desired_urls);
  base::Value::List* urls_to_remove_debug =
      debug_info.EnsureList("urls_to_remove");
  for (const GURL& url_to_remove : urls_to_remove) {
    urls_to_remove_debug->Append(url_to_remove.spec());
  }

  // Run callback immediately if there's no work to be done.
  if (urls_to_remove.empty() && desired_apps_install_options.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::map<GURL, InstallResult>(),
                       std::map<GURL, webapps::UninstallResultCode>()));
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
    ContinueSynchronization(install_source);
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
  CHECK(source_and_request != synchronize_requests_.end(),
        base::NotFatalUntil::M130);
  SynchronizeRequest& request = source_and_request->second;
  request.install_results[install_url] = std::move(result);
  --request.remaining_install_requests;
  DCHECK_GE(request.remaining_install_requests, 0);

  ContinueSynchronization(source);
}

void ExternallyManagedAppManager::UninstallForSynchronizeCallback(
    ExternalInstallSource source,
    const GURL& install_url,
    webapps::UninstallResultCode code) {
  auto source_and_request = synchronize_requests_.find(source);
  CHECK(source_and_request != synchronize_requests_.end(),
        base::NotFatalUntil::M130);
  SynchronizeRequest& request = source_and_request->second;
  request.uninstall_results[install_url] = code;
  --request.remaining_uninstall_requests;
  DCHECK_GE(request.remaining_uninstall_requests, 0);

  ContinueSynchronization(source);
}

void ExternallyManagedAppManager::ContinueSynchronization(
    ExternalInstallSource source) {
  auto source_and_request = synchronize_requests_.find(source);
  CHECK(source_and_request != synchronize_requests_.end(),
        base::NotFatalUntil::M130);

  SynchronizeRequest& request = source_and_request->second;

  if (request.remaining_uninstall_requests > 0) {
    return;
  }

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

  if (request.remaining_install_requests > 0) {
    return;
  }

  provider_->scheduler().ScheduleDedupeInstallUrls(
      base::BindOnce(&ExternallyManagedAppManager::CompleteSynchronization,
                     weak_ptr_factory_.GetWeakPtr(), source));
}

void ExternallyManagedAppManager::CompleteSynchronization(
    ExternalInstallSource source) {
  auto source_and_request = synchronize_requests_.find(source);
  CHECK(source_and_request != synchronize_requests_.end(),
        base::NotFatalUntil::M130);

  SynchronizeRequest& request = source_and_request->second;
  CHECK(request.callback);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(request.callback),
                                std::move(request.install_results),
                                std::move(request.uninstall_results)));
  synchronize_requests_.erase(source);
}

void ExternallyManagedAppManager::ClearSynchronizeRequestsForTesting() {
  synchronize_requests_.erase(synchronize_requests_.begin(),
                              synchronize_requests_.end());
}

std::ostream& operator<<(
    std::ostream& out,
    const ExternallyManagedAppManager::InstallResult& install_result) {
  base::Value::Dict output;
  output.Set("code", base::ToString(install_result.code));
  output.Set("app_id", base::ToString(install_result.app_id));
  output.Set("did_uninstall_and_replace",
             install_result.did_uninstall_and_replace);
  out << output.DebugString();
  return out;
}

}  // namespace web_app
