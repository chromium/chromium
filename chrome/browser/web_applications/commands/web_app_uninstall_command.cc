// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"

#include <memory>
#include <sstream>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/remove_web_app_job.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"

namespace web_app {

namespace {

bool CanUninstallAllManagementSources(
    webapps::WebappUninstallSource uninstall_source) {
  // Check that the source was from a known 'user' or allowed ones such
  // as kMigration.
  return uninstall_source == webapps::WebappUninstallSource::kUnknown ||
         uninstall_source == webapps::WebappUninstallSource::kAppMenu ||
         uninstall_source == webapps::WebappUninstallSource::kAppsPage ||
         uninstall_source == webapps::WebappUninstallSource::kOsSettings ||
         uninstall_source == webapps::WebappUninstallSource::kAppManagement ||
         uninstall_source == webapps::WebappUninstallSource::kMigration ||
         uninstall_source == webapps::WebappUninstallSource::kAppList ||
         uninstall_source == webapps::WebappUninstallSource::kShelf ||
         uninstall_source == webapps::WebappUninstallSource::kSync ||
         uninstall_source == webapps::WebappUninstallSource::kStartupCleanup ||
         uninstall_source == webapps::WebappUninstallSource::kTestCleanup;
}

enum class InstallUrlActionNeeded {
  kNone,
  kRemoveInstallUrl,
  kRemoveInstallSource,
};

InstallUrlActionNeeded GetInstallUrlActionNeeded(
    const WebApp::ExternalConfigMap& config_map,
    const UninstallRequest& request) {
  auto it = config_map.find(*request.install_source());
  if (it == config_map.end()) {
    return InstallUrlActionNeeded::kNone;
  }

  const WebApp::ExternalManagementConfig& config = it->second;
  const base::flat_set<GURL>& install_urls = config.install_urls;
  if (install_urls.empty()) {
    // TODO(crbug.com/1427340): Return a different UninstallResultCode
    // for this case and log it in metrics.
    return InstallUrlActionNeeded::kRemoveInstallSource;
  }
  if (install_urls.size() == 1 &&
      install_urls.contains(*request.install_url())) {
    return InstallUrlActionNeeded::kRemoveInstallSource;
  }

  return InstallUrlActionNeeded::kRemoveInstallUrl;
}

enum class InstallSourceActionNeeded {
  kNone,
  kRemoveInstallSource,
  kRemoveApp,
};

InstallSourceActionNeeded GetInstallSourceActionNeeded(
    const WebAppSources& sources,
    WebAppManagement::Type install_source) {
  if (sources.none()) {
    // TODO(crbug.com/1427340): Return a different UninstallResultCode
    // for this case and log it in metrics.
    return InstallSourceActionNeeded::kRemoveApp;
  }

  if (!sources[install_source]) {
    return InstallSourceActionNeeded::kNone;
  }

  if (sources.count() > 1) {
    return InstallSourceActionNeeded::kRemoveInstallSource;
  }

  return InstallSourceActionNeeded::kRemoveApp;
}

}  // namespace

WebAppUninstallCommand::WebAppUninstallCommand(UninstallRequest request,
                                               UninstallWebAppCallback callback,
                                               Profile& profile)
    : WebAppCommandTemplate<AllAppsLock>("WebAppUninstallCommand"),
      lock_description_(std::make_unique<AllAppsLockDescription>()),
      initial_request_app_id_(request.app_id()),
      callback_(std::move(callback)),
      profile_(profile) {
  webapps::InstallableMetrics::TrackUninstallEvent(request.uninstall_source());

  request_queue_.push_back(std::move(request));
}

WebAppUninstallCommand::~WebAppUninstallCommand() = default;

void WebAppUninstallCommand::StartWithLock(std::unique_ptr<AllAppsLock> lock) {
  lock_ = std::move(lock);
  ProcessRequestQueueOrComplete();
}

void WebAppUninstallCommand::OnShutdown() {
  CHECK(callback_);
  base::UmaHistogramBoolean("WebApp.Uninstall.Result", false);
  SignalCompletionAndSelfDestruct(
      CommandResult::kShutdown,
      base::BindOnce(std::move(callback_),
                     webapps::UninstallResultCode::kError));
  return;
}

const LockDescription& WebAppUninstallCommand::lock_description() const {
  return *lock_description_;
}

base::Value WebAppUninstallCommand::ToDebugValue() const {
  base::Value::Dict dict;

  base::Value::List requests;
  for (const UninstallRequest& request : request_queue_) {
    requests.Append(request.ToDebugValue());
  }
  dict.Set("request_queue", std::move(requests));

  dict.Set("initial_request_app_id", initial_request_app_id_);

  base::Value::List results;
  for (const std::pair<AppId, webapps::UninstallResultCode>& pair :
       uninstall_results_) {
    base::Value::Dict result;
    result.Set(pair.first, base::ToString(pair.second));
  }
  dict.Set("uninstall_results", std::move(results));

  dict.Set("active_remove_web_app_job", bool(active_remove_web_app_job_));

  return base::Value(std::move(dict));
}

////////////////////////////////////////////////////////////////////////////////
// Process request queue.
////////////////////////////////////////////////////////////////////////////////

void WebAppUninstallCommand::ProcessRequestQueueOrComplete() {
  if (request_queue_.empty()) {
    CHECK(base::Contains(uninstall_results_, initial_request_app_id_));
    webapps::UninstallResultCode code =
        uninstall_results_[initial_request_app_id_];
    base::UmaHistogramBoolean("WebApp.Uninstall.Result",
                              webapps::UninstallSucceeded(code));
    SignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                    base::BindOnce(std::move(callback_), code));
    return;
  }

  UninstallRequest request = std::move(request_queue_.front());
  request_queue_.pop_front();

  auto request_complete_callback =
      base::BindOnce(&WebAppUninstallCommand::RequestComplete,
                     weak_factory_.GetWeakPtr(), request.app_id());

  if (request.install_url()) {
    RemoveInstallUrl(request, std::move(request_complete_callback));
  } else if (request.install_source()) {
    RemoveInstallSource(request, std::move(request_complete_callback));
  } else {
    RemoveApp(request, std::move(request_complete_callback));
  }
}

void WebAppUninstallCommand::RequestComplete(
    AppId app_id,
    webapps::UninstallResultCode code) {
  CHECK(!base::Contains(uninstall_results_, app_id));
  uninstall_results_[app_id] = code;
  ProcessRequestQueueOrComplete();
}

////////////////////////////////////////////////////////////////////////////////
// Remove install URL.
////////////////////////////////////////////////////////////////////////////////

void WebAppUninstallCommand::RemoveInstallUrl(
    const UninstallRequest& request,
    RequestCompleteCallback callback) {
  CHECK(request.install_source());
  CHECK(request.install_url());

  const WebApp* app = lock_->registrar().GetAppById(request.app_id());
  if (!app) {
    std::move(callback).Run(webapps::UninstallResultCode::kNoAppToUninstall);
    return;
  }

  switch (GetInstallUrlActionNeeded(app->management_to_external_config_map(),
                                    request)) {
    case InstallUrlActionNeeded::kNone:
      std::move(callback).Run(webapps::UninstallResultCode::kSuccess);
      return;

    case InstallUrlActionNeeded::kRemoveInstallUrl: {
      {
        ScopedRegistryUpdate update(&lock_->sync_bridge());
        CHECK(update->UpdateApp(request.app_id())
                  ->RemoveInstallUrlForSource(*request.install_source(),
                                              *request.install_url()));
      }

      std::move(callback).Run(webapps::UninstallResultCode::kSuccess);
      return;
    }

    case InstallUrlActionNeeded::kRemoveInstallSource:
      RemoveInstallSource(
          UninstallRequest(base::PassKey<WebAppUninstallCommand>(),
                           /*is_sub_request=*/true, request.uninstall_source(),
                           request.app_id(), request.install_source()),
          std::move(callback));
      return;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Remove install source.
////////////////////////////////////////////////////////////////////////////////

void WebAppUninstallCommand::RemoveInstallSource(
    const UninstallRequest& request,
    RequestCompleteCallback callback) {
  CHECK(request.install_source());
  CHECK(!request.install_url());

  const WebApp* app = lock_->registrar().GetAppById(request.app_id());
  if (!app) {
    std::move(callback).Run(webapps::UninstallResultCode::kNoAppToUninstall);
    return;
  }

  WebAppManagement::Type install_source = *request.install_source();
  switch (GetInstallSourceActionNeeded(app->GetSources(), install_source)) {
    case InstallSourceActionNeeded::kNone:
      // TODO(crbug.com/1427340): Return a different UninstallResultCode
      // for when no action is taken instead of being overly specific to the "no
      // app" case.
      std::move(callback).Run(webapps::UninstallResultCode::kNoAppToUninstall);
      return;

    case InstallSourceActionNeeded::kRemoveInstallSource:
      MaybeRegisterOsUninstall(
          app, install_source, lock_->os_integration_manager(),
          base::BindOnce(
              &WebAppUninstallCommand::RemoveInstallSourceFromDatabase,
              weak_factory_.GetWeakPtr(), request.app_id(),
              *request.install_source(), std::move(callback)));

      return;

    case InstallSourceActionNeeded::kRemoveApp:
      RemoveApp(UninstallRequest(base::PassKey<WebAppUninstallCommand>(),
                                 /*is_sub_request=*/true,
                                 request.uninstall_source(), request.app_id()),
                std::move(callback));
      return;
  }
}

void WebAppUninstallCommand::RemoveInstallSourceFromDatabase(
    AppId app_id,
    WebAppManagement::Type install_source,
    RequestCompleteCallback callback,
    OsHooksErrors os_hooks_errors) {
  {
    ScopedRegistryUpdate update(&lock_->sync_bridge());
    WebApp* app = update->UpdateApp(app_id);
    app->RemoveSource(install_source);
    if (install_source == WebAppManagement::kSubApp) {
      app->SetParentAppId(absl::nullopt);
    }
    // TODO(crbug.com/1447308): Make sync uninstall not synchronously
    // remove its sync install source even while a command has an app lock so
    // that we can CHECK(app->HasAnySources()) here.
  }

  lock_->install_manager().NotifyWebAppSourceRemovedForTesting(app_id);

  std::move(callback).Run(webapps::UninstallResultCode::kSuccess);
}

////////////////////////////////////////////////////////////////////////////////
// Remove app.
////////////////////////////////////////////////////////////////////////////////

void WebAppUninstallCommand::RemoveApp(const UninstallRequest& request,
                                       RequestCompleteCallback callback) {
  CHECK(!request.install_source().has_value());
  CHECK(!request.install_url().has_value());

  if (!request.is_sub_request()) {
    CHECK(CanUninstallAllManagementSources(request.uninstall_source()));
  }

  const WebApp* app = lock_->registrar().GetAppById(request.app_id());
  if (!app) {
    std::move(callback).Run(webapps::UninstallResultCode::kNoAppToUninstall);
    return;
  }

  if (!request.is_sub_request()) {
    // The following CHECK streamlines the user uninstall and sync uninstall
    // flow, because for sync uninstalls, the web_app source is removed before
    // being synced, so the first condition fails by the time an Uninstall is
    // invoked.
    // TODO(crbug.com/1447308): Checking kSync shouldn't be needed once
    // this issue is resolved.
    // TODO(crbug.com/1427340): Change this to be:
    // if (uninstall_source is user initiated) {
    //   CHECK(user can uninstall);
    //   Add to user uninstalled prefs.
    // }
    CHECK(app->CanUserUninstallWebApp() ||
          request.uninstall_source() == webapps::WebappUninstallSource::kSync);

    if (app->IsPreinstalledApp()) {
      // Update the default uninstalled web_app prefs if it is a preinstalled
      // app but being removed by user.
      const WebApp::ExternalConfigMap& config_map =
          app->management_to_external_config_map();
      auto it = config_map.find(WebAppManagement::kDefault);
      if (it != config_map.end()) {
        UserUninstalledPreinstalledWebAppPrefs(profile_->GetPrefs())
            .Add(request.app_id(), it->second.install_urls);
      } else {
        base::UmaHistogramBoolean(
            "WebApp.Preinstalled.ExternalConfigMapAbsentDuringUninstall", true);
      }
    }
  }

  for (const AppId& sub_app_id :
       lock_->registrar().GetAllSubAppIds(request.app_id())) {
    request_queue_.emplace_back(
        base::PassKey<WebAppUninstallCommand>(),
        /*is_sub_request=*/true,
        webapps::WebappUninstallSource::kParentUninstall, sub_app_id,
        WebAppManagement::kSubApp);
  }

  active_remove_web_app_job_ = RemoveWebAppJob::Start(
      request.uninstall_source(), request.app_id(), *lock_, profile_.get(),
      base::BindOnce(&WebAppUninstallCommand::OnUninstallJobComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebAppUninstallCommand::OnUninstallJobComplete(
    RequestCompleteCallback callback,
    bool success) {
  CHECK(active_remove_web_app_job_);
  active_remove_web_app_job_.reset();
  std::move(callback).Run(success ? webapps::UninstallResultCode::kSuccess
                                  : webapps::UninstallResultCode::kError);
}

}  // namespace web_app
