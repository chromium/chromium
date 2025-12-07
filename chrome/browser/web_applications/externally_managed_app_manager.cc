// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_manager.h"

#include <algorithm>
#include <map>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/stl_util.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_registration_task.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace web_app {

namespace {

// TODO(crbug.com/408163317): Do not use, this is an implementation detail and
// will be removed later.
bool& DropRequestsForTesting() {
  static bool drop_requests_for_testing_ = false;
  return drop_requests_for_testing_;
}

}  // namespace

ExternallyManagedAppManagerInstallResult::
    ExternallyManagedAppManagerInstallResult() = default;

ExternallyManagedAppManagerInstallResult::
    ExternallyManagedAppManagerInstallResult(
        webapps::InstallResultCode code,
        std::optional<webapps::AppId> app_id,
        bool did_uninstall_and_replace)
    : code(code),
      app_id(std::move(app_id)),
      did_uninstall_and_replace(did_uninstall_and_replace) {}

ExternallyManagedAppManagerInstallResult::
    ExternallyManagedAppManagerInstallResult(
        const ExternallyManagedAppManagerInstallResult&) = default;

ExternallyManagedAppManagerInstallResult::
    ~ExternallyManagedAppManagerInstallResult() = default;

bool ExternallyManagedAppManagerInstallResult::operator==(
    const ExternallyManagedAppManagerInstallResult& other) const {
  return std::tie(code, app_id, did_uninstall_and_replace) ==
         std::tie(other.code, other.app_id, other.did_uninstall_and_replace);
}

ExternallyManagedAppManager::ScopedDropRequestsForTesting::
    ScopedDropRequestsForTesting() {
  CHECK_IS_TEST();
  DropRequestsForTesting() = true;  // IN-TEST
}

ExternallyManagedAppManager::ScopedDropRequestsForTesting::
    ~ScopedDropRequestsForTesting() {
  DropRequestsForTesting() = false;  // IN-TEST
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

struct ExternallyManagedAppManager::ExternalInstallMetadata {
  ExternalInstallMetadata(ExternalInstallOptions options,
                          OnceInstallCallback callback)
      : options(std::move(options)), callback(std::move(callback)) {}
  ~ExternalInstallMetadata() = default;

  ExternalInstallOptions options;
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
  pending_installs_metadata_.push_front(
      std::make_unique<ExternalInstallMetadata>(std::move(install_options),
                                                std::move(callback)));

  PostMaybeStartNext();
}

void ExternallyManagedAppManager::Install(
    ExternalInstallOptions install_options,
    OnceInstallCallback callback) {
  pending_installs_metadata_.push_back(
      std::make_unique<ExternalInstallMetadata>(std::move(install_options),
                                                std::move(callback)));

  PostMaybeStartNext();
}

void ExternallyManagedAppManager::SynchronizeInstalledApps(
    std::vector<ExternalInstallOptions> desired_apps_install_options,
    ExternalInstallSource install_source,
    SynchronizeCallback callback) {
  CHECK(callback);
  CHECK(std::ranges::all_of(
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
  pending_installs_metadata_.clear();
  url_loader_.reset();
  // `current_install_` keeps a pointer to `web_contents_` so destroy it before
  // releasing the WebContents.
  current_install_metadata_.reset();
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
  DCHECK(pending_installs_metadata_.empty());
  DCHECK(!current_install_metadata_);

  web_contents_.reset();
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
  if (current_install_metadata_ || IsShuttingDown()) {
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
  if (current_install_metadata_ || IsShuttingDown()) {
    return;
  }

  while (!pending_installs_metadata_.empty()) {
    std::unique_ptr<ExternalInstallMetadata> front =
        std::move(pending_installs_metadata_.front());
    pending_installs_metadata_.pop_front();

    const ExternalInstallOptions& install_options = front->options;

    CHECK(install_options.install_url.is_valid());
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

    std::optional<proto::InstallState> install_state =
        lock.registrar().GetInstallState(app_id.value());
    if (install_state == web_app::proto::INSTALLED_WITH_OS_INTEGRATION ||
        install_state == web_app::proto::INSTALLED_WITHOUT_OS_INTEGRATION) {
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
      }

      debug_value.Set("simple_source_addition", true);
      // Add install source before returning the result.
      ScopedRegistryUpdate update = lock.sync_bridge().BeginUpdate();
      WebApp* app_to_update = update->UpdateApp(app_id.value());
      app_to_update->AddSource(
          ConvertExternalInstallSourceToSource(install_options.install_source));
      app_to_update->AddInstallURLToManagementExternalConfigMap(
          ConvertExternalInstallSourceToSource(install_options.install_source),
          install_options.install_url);

      std::move(front->callback)
          .Run(install_options.install_url,
               ExternallyManagedAppManagerInstallResult(
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
  DCHECK(!current_install_metadata_);

  if (current_registration_ || RunNextRegistration()) {
    return;
  }

  ReleaseWebContents();
}

void ExternallyManagedAppManager::StartInstallationTask(
    std::unique_ptr<ExternalInstallMetadata> external_install_metadata,
    std::optional<webapps::AppId> installed_placeholder_app_id) {
  if (IsShuttingDown()) {
    return;
  }
  DCHECK(!current_install_metadata_);
  DCHECK(!is_in_shutdown_);
  if (current_registration_) {
    // Preempt current registration.
    pending_registrations_.push_front(
        {current_registration_->install_url(),
         current_registration_->registration_timeout()});
    current_registration_.reset();
  }

  current_install_metadata_ = std::move(external_install_metadata);
  CreateWebContentsIfNecessary();
  provider_->scheduler().InstallExternallyManagedApp(
      current_install_metadata_->options,
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
    ExternallyManagedAppManagerInstallResult result) {
  if (result.app_id && IsSuccess(result.code)) {
    MaybeEnqueueServiceWorkerRegistration(current_install_metadata_->options);
  }

  // Post a task to avoid webapps::InstallableManager crashing and do so before
  // running the callback in case the callback tries to install another
  // app.
  PostMaybeStartNext();

  std::unique_ptr<ExternalInstallMetadata> metadata_and_callback;
  metadata_and_callback.swap(current_install_metadata_);
  std::move(metadata_and_callback->callback)
      .Run(metadata_and_callback->options.install_url, result);
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
  if (url.GetScheme() == content::kChromeUIScheme) {
    return;
  }
  if (url.GetScheme() == content::kChromeUIUntrustedScheme) {
    return;
  }

  pending_registrations_.push_back(
      {url, install_options.service_worker_registration_timeout});
}

bool ExternallyManagedAppManager::IsShuttingDown() {
  return is_in_shutdown_ || profile()->ShutdownStarted();
}

void ExternallyManagedAppManager::InstallApps(
    std::vector<ExternalInstallOptions> install_options_list,
    const RepeatingInstallCallback& callback) {
  if (DropRequestsForTesting()) {
    CHECK_IS_TEST();
    return;
  }

  for (auto& install_options : install_options_list) {
    pending_installs_metadata_.push_back(
        std::make_unique<ExternalInstallMetadata>(std::move(install_options),
                                                  callback));
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

void ExternallyManagedAppManager::SynchronizeInstalledAppsOnLockAcquired(
    std::vector<ExternalInstallOptions> desired_apps_install_options,
    ExternalInstallSource install_source,
    SynchronizeCallback callback,
    AllAppsLock& lock,
    base::Value::Dict& debug_info) {
  CHECK(callback);
  debug_info.Set("install_source", base::ToString(install_source));

  auto is_app_installed_by_other_sources_or_display_modes =
      [&](const webapps::AppId& app_id,
          std::optional<mojom::UserDisplayMode> desired_display_mode) {
        if (!lock.registrar().IsInstallState(
                app_id,
                {web_app::proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
                 web_app::proto::InstallState::
                     INSTALLED_WITH_OS_INTEGRATION})) {
          return /*keep_old_app=*/false;
        }
        const WebApp* app = lock.registrar().GetAppById(app_id);
        CHECK(app);
        if (!app->HasOnlySource(
                ConvertExternalInstallSourceToSource(install_source))) {
          return /*keep_old_app=*/true;
        }
        if (desired_display_mode == mojom::UserDisplayMode::kBrowser &&
            app->user_display_mode() != desired_display_mode) {
          return /*keep_old_app=*/true;
        }
        return /*keep_old_app=*/false;
      };
  auto get_install_urls_for_source = [&](const webapps::AppId& app_id) {
    const WebApp* app = lock.registrar().GetAppById(app_id);
    CHECK(app);

    absl::flat_hash_set<GURL> install_urls;
    const WebApp::ExternalConfigMap& config_map =
        app->management_to_external_config_map();
    auto it =
        config_map.find(ConvertExternalInstallSourceToSource(install_source));
    if (it != config_map.end() && !it->second.install_urls.empty()) {
      install_urls = absl::flat_hash_set<GURL>(it->second.install_urls.begin(),
                                               it->second.install_urls.end());
    }
    return install_urls;
  };

  absl::flat_hash_set<GURL> desired_urls;
  absl::flat_hash_set<GURL> backup_installed_urls;
  std::vector<ExternalInstallOptions> desired_installs;
  for (auto& info : desired_apps_install_options) {
    std::optional<webapps::AppId> app_to_maybe_replace =
        info.only_uninstall_and_replace_when_compatible();
    if (app_to_maybe_replace.has_value() &&
        is_app_installed_by_other_sources_or_display_modes(
            *app_to_maybe_replace, info.user_display_mode)) {
      base::ListValue backup_urls_debug;
      // Add install URLs from the app being replaced to backup_installed_urls
      // to make sure we don't uninstall/remove our source from the already
      // installed app.
      for (const GURL& url :
           get_install_urls_for_source(*app_to_maybe_replace)) {
        backup_installed_urls.insert(url);
        backup_urls_debug.Append(url.spec());
      }
      debug_info.EnsureList("backup_apps_found")
          ->Append(base::DictValue()
                       .Set("using", *app_to_maybe_replace)
                       .Set("for", info.install_url.spec())
                       .Set("backup_urls", std::move(backup_urls_debug)));
    } else {
      desired_urls.insert(info.install_url);
      debug_info.EnsureList("desired_install_urls")
          ->Append(info.install_url.spec());
      desired_installs.push_back(std::move(info));
    }
  }

  absl::flat_hash_set<GURL> installed_urls;
  for (const auto& [app_id, install_urls] :
       lock.registrar().GetExternallyInstalledApps(install_source)) {
    for (const GURL& url : install_urls) {
      installed_urls.insert(url);
    }
  }

  std::vector<GURL> urls_to_remove;
  for (const GURL& installed_url : installed_urls) {
    if (!desired_urls.contains(installed_url) &&
        !backup_installed_urls.contains(installed_url)) {
      debug_info.EnsureList("urls_to_remove")->Append(installed_url.spec());
      urls_to_remove.push_back(installed_url);
    }
  }

  // Run callback immediately if there's no work to be done.
  if (urls_to_remove.empty() && desired_installs.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::map<GURL, InstallResult>(),
                       std::map<GURL, webapps::UninstallResultCode>()));
    return;
  }

  // Add the callback to a map and call once all installs/uninstalls finish.
  synchronize_requests_.insert_or_assign(
      install_source,
      SynchronizeRequest(std::move(callback), std::move(desired_installs),
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
    ExternallyManagedAppManagerInstallResult result) {
  if (!IsSuccess(result.code)) {
    LOG(ERROR) << install_url << " from install source "
               << static_cast<int>(source) << " failed to install with reason "
               << static_cast<int>(result.code);
  }

  auto source_and_request = synchronize_requests_.find(source);
  CHECK(source_and_request != synchronize_requests_.end());
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
  CHECK(source_and_request != synchronize_requests_.end());
  SynchronizeRequest& request = source_and_request->second;
  request.uninstall_results[install_url] = code;
  --request.remaining_uninstall_requests;
  DCHECK_GE(request.remaining_uninstall_requests, 0);

  ContinueSynchronization(source);
}

void ExternallyManagedAppManager::ContinueSynchronization(
    ExternalInstallSource source) {
  auto source_and_request = synchronize_requests_.find(source);
  CHECK(source_and_request != synchronize_requests_.end());

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
  CHECK(source_and_request != synchronize_requests_.end());

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
    const ExternallyManagedAppManagerInstallResult& install_result) {
  base::Value::Dict output;
  output.Set("code", base::ToString(install_result.code));
  output.Set("app_id", base::ToString(install_result.app_id));
  output.Set("did_uninstall_and_replace",
             install_result.did_uninstall_and_replace);
  out << output.DebugString();
  return out;
}

}  // namespace web_app
