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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolation_prefs_utils.h"
#include "chrome/browser/web_applications/locks/full_system_lock.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_uninstall_job.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"

namespace web_app {

namespace {

bool CanUninstallAllManagementSources(
    const webapps::WebappUninstallSource& uninstall_source) {
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

auto StreamableToString = [](const auto& value) {
  std::stringstream ss;
  ss << value;
  return ss.str();
};

}  // namespace

WebAppUninstallCommand::WebAppUninstallCommand(
    const AppId& app_id,
    absl::optional<WebAppManagement::Type> management_type_or_all,
    webapps::WebappUninstallSource uninstall_source,
    UninstallWebAppCallback callback,
    Profile* profile)
    : WebAppCommandTemplate<FullSystemLock>("WebAppUninstallCommand"),
      lock_description_(std::make_unique<FullSystemLockDescription>()),
      app_id_(app_id),
      callback_(std::move(callback)),
      profile_prefs_(profile->GetPrefs()) {
  // Initializing data for uninstallation tracking.
  queued_uninstalls_.emplace_back(app_id_, management_type_or_all,
                                  uninstall_source);

  webapps::InstallableMetrics::TrackUninstallEvent(uninstall_source);
}

WebAppUninstallCommand::~WebAppUninstallCommand() = default;

void WebAppUninstallCommand::StartWithLock(
    std::unique_ptr<FullSystemLock> lock) {
  lock_ = std::move(lock);

  while (!queued_uninstalls_.empty()) {
    DCHECK(!all_uninstalled_queued_);
    const UninstallInfo current_uninstall =
        std::move(queued_uninstalls_.back());
    queued_uninstalls_.pop_back();
    AppendUninstallInfoToDebugLog(current_uninstall);

    const AppId& app_id = current_uninstall.app_id;
    const WebApp* app = lock_->registrar().GetAppById(app_id);
    if (!app) {
      uninstall_results_[app_id] =
          webapps::UninstallResultCode::kNoAppToUninstall;
      AppendUninstallResultsToDebugLog(app_id);
      continue;
    }

    // This contains the external uninstall logic.
    if (current_uninstall.management_type_or_all.has_value()) {
      const WebAppManagement::Type source =
          current_uninstall.management_type_or_all.value();
      // If there is more than a single source, then we can just remove the
      // source from the web_app DB. Else we end up calling Uninstall() at the
      // end.
      if (!app->HasOnlySource(source)) {
        // There is a chance that removed source type is NOT user uninstallable
        // but the remaining source (after removal) types are user
        // uninstallable. In this case, the following call will register os
        // uninstallation.
        apps_pending_uninstall_[app_id] = nullptr;
        MaybeRegisterOsUninstall(
            app, source, lock_->os_integration_manager(),
            base::BindOnce(&WebAppUninstallCommand::
                               RemoveManagementTypeAfterOsUninstallRegistration,
                           weak_factory_.GetWeakPtr(), app_id, source,
                           current_uninstall.uninstall_source));
      } else {
        Uninstall(app_id, current_uninstall.uninstall_source);
      }
    } else {
      // This contains the user uninstall and sync uninstall logic.
      DCHECK(
          CanUninstallAllManagementSources(current_uninstall.uninstall_source));
      // The following DCHECK streamlines the user uninstall and sync uninstall
      // flow, because for sync uninstalls, the web_app source is removed before
      // being synced, so the first condition fails by the time an Uninstall is
      // invoked.
      DCHECK(app->CanUserUninstallWebApp() ||
             current_uninstall.uninstall_source ==
                 webapps::WebappUninstallSource::kSync);
      if (app->IsPreinstalledApp()) {
        // Update the default uninstalled web_app prefs if it is a preinstalled
        // app but being removed by user.
        const WebApp::ExternalConfigMap& config_map =
            app->management_to_external_config_map();
        auto it = config_map.find(WebAppManagement::kDefault);
        if (it != config_map.end()) {
          UserUninstalledPreinstalledWebAppPrefs(profile_prefs_)
              .Add(app_id, it->second.install_urls);
        } else {
          base::UmaHistogramBoolean(
              "WebApp.Preinstalled.ExternalConfigMapAbsentDuringUninstall",
              true);
        }
      }
      Uninstall(app_id, current_uninstall.uninstall_source);
    }
  }
  all_uninstalled_queued_ = true;
  MaybeFinishUninstallAndDestruct();
}

void WebAppUninstallCommand::OnSyncSourceRemoved() {
  // TODO(crbug.com/1320086): remove after uninstall from sync is async.
  Abort(webapps::UninstallResultCode::kNoAppToUninstall);
  return;
}

void WebAppUninstallCommand::OnShutdown() {
  Abort(webapps::UninstallResultCode::kError);
  return;
}

const LockDescription& WebAppUninstallCommand::lock_description() const {
  return *lock_description_;
}

base::Value WebAppUninstallCommand::ToDebugValue() const {
  base::Value::Dict uninstall_info;
  uninstall_info.Set("command_data", debug_log_.Clone());
  return base::Value(std::move(uninstall_info));
}

void WebAppUninstallCommand::SetRemoveManagementTypeCallbackForTesting(
    RemoveManagementTypeCallback callback) {
  management_type_removed_callback_for_testing_ = std::move(callback);
}

WebAppUninstallCommand::UninstallInfo::UninstallInfo(
    AppId app_id,
    absl::optional<WebAppManagement::Type> management_type_or_all,
    webapps::WebappUninstallSource uninstall_source)
    : app_id(app_id),
      management_type_or_all(management_type_or_all),
      uninstall_source(uninstall_source) {}

WebAppUninstallCommand::UninstallInfo::~UninstallInfo() = default;

WebAppUninstallCommand::UninstallInfo::UninstallInfo(
    const UninstallInfo& uninstall_info) = default;

WebAppUninstallCommand::UninstallInfo::UninstallInfo(
    UninstallInfo&& uninstall_info) = default;

void WebAppUninstallCommand::AppendUninstallInfoToDebugLog(
    const UninstallInfo& uninstall_info) {
  base::Value::Dict source_info;
  if (uninstall_info.management_type_or_all.has_value()) {
    source_info.Set(
        "management_type",
        StreamableToString(uninstall_info.management_type_or_all.value()));
  }
  source_info.Set("uninstall_source", ConvertUninstallSourceToStringType(
                                          uninstall_info.uninstall_source));
  debug_log_.Set(uninstall_info.app_id, base::Value(std::move(source_info)));
}

void WebAppUninstallCommand::AppendUninstallResultsToDebugLog(
    const AppId& app_id) {
  base::Value::Dict* app_dict = debug_log_.FindDict(app_id);
  DCHECK(app_dict);
  app_dict->Set("uninstall_result", webapps::ConvertUninstallResultCodeToString(
                                        uninstall_results_[app_id]));
}

void WebAppUninstallCommand::Abort(webapps::UninstallResultCode code) {
  base::UmaHistogramBoolean("WebApp.Uninstall.Result",
                            (code == webapps::UninstallResultCode::kSuccess));
  if (!callback_)
    return;
  SignalCompletionAndSelfDestruct(CommandResult::kFailure,
                                  base::BindOnce(std::move(callback_), code));
}

void WebAppUninstallCommand::Uninstall(
    const AppId& app_id,
    const webapps::WebappUninstallSource& uninstall_source) {
  QueueSubAppsForUninstallIfAny(app_id);

  auto uninstall_job = WebAppUninstallJob::CreateAndStart(
      app_id,
      url::Origin::Create(lock_->registrar().GetAppById(app_id)->start_url()),
      base::BindOnce(&WebAppUninstallCommand::OnSingleUninstallComplete,
                     weak_factory_.GetWeakPtr(), app_id, uninstall_source),
      lock_->os_integration_manager(), lock_->sync_bridge(),
      lock_->icon_manager(), lock_->registrar(), lock_->install_manager(),
      lock_->translation_manager(), *profile_prefs_);
  apps_pending_uninstall_[app_id] = std::move(uninstall_job);
}

void WebAppUninstallCommand::QueueSubAppsForUninstallIfAny(
    const AppId& app_id) {
  std::vector<AppId> sub_app_ids = lock_->registrar().GetAllSubAppIds(app_id);
  for (const AppId& sub_app_id : sub_app_ids) {
    queued_uninstalls_.emplace_back(
        sub_app_id, WebAppManagement::kSubApp,
        webapps::WebappUninstallSource::kParentUninstall);
  }
}

void WebAppUninstallCommand::RemoveManagementTypeAfterOsUninstallRegistration(
    const AppId& app_id,
    const WebAppManagement::Type& management_type,
    const webapps::WebappUninstallSource& uninstall_source,
    OsHooksErrors os_hooks_errors) {
  {
    ScopedRegistryUpdate update(&lock_->sync_bridge());
    WebApp* app_to_update = update->UpdateApp(app_id);
    app_to_update->RemoveSource(management_type);
    if (management_type == WebAppManagement::kSubApp) {
      app_to_update->SetParentAppId(absl::nullopt);
    }
  }

  if (management_type_removed_callback_for_testing_)
    std::move(management_type_removed_callback_for_testing_).Run(app_id);

  // Registering an OS uninstall is also an "uninstall", so the
  // state is updated for the command.
  OnSingleUninstallComplete(app_id, uninstall_source,
                            webapps::UninstallResultCode::kSuccess);
}

void WebAppUninstallCommand::OnSingleUninstallComplete(
    const AppId& app_id,
    const webapps::WebappUninstallSource& source,
    webapps::UninstallResultCode code) {
  DCHECK(base::Contains(apps_pending_uninstall_, app_id));
  apps_pending_uninstall_.erase(app_id);

  if (source == webapps::WebappUninstallSource::kSync) {
    base::UmaHistogramBoolean("Webapp.SyncInitiatedUninstallResult",
                              code == webapps::UninstallResultCode::kSuccess);
  }
  uninstall_results_[app_id] = code;
  AppendUninstallResultsToDebugLog(app_id);
  MaybeFinishUninstallAndDestruct();
}

void WebAppUninstallCommand::MaybeFinishUninstallAndDestruct() {
  if (apps_pending_uninstall_.empty() && all_uninstalled_queued_) {
    // All uninstall jobs have finished.
    SignalCompletionAndSelfDestruct(
        CommandResult::kSuccess,
        base::BindOnce(std::move(callback_), uninstall_results_[app_id_]));
  }
}

}  // namespace web_app
