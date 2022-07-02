// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolation_prefs_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

WebAppUninstallCommand::WebAppUninstallCommand(
    const AppId& app_id,
    const url::Origin& app_origin,
    Profile* profile,
    OsIntegrationManager* os_integration_manager,
    WebAppSyncBridge* sync_bridge,
    WebAppIconManager* icon_manager,
    WebAppRegistrar* registrar,
    WebAppInstallManager* install_manager,
    WebAppInstallFinalizer* install_finalizer,
    WebAppTranslationManager* translation_manager,
    webapps::WebappUninstallSource source,
    UninstallWebAppCallback callback)
    : WebAppCommand(WebAppCommandLock::CreateForAppLock({app_id})),
      app_id_(app_id),
      app_origin_(app_origin),
      source_(source),
      callback_(std::move(callback)),
      os_integration_manager_(os_integration_manager),
      sync_bridge_(sync_bridge),
      icon_manager_(icon_manager),
      registrar_(registrar),
      install_manager_(install_manager),
      install_finalizer_(install_finalizer),
      translation_manager_(translation_manager),
      profile_prefs_(profile->GetPrefs()) {}

WebAppUninstallCommand::~WebAppUninstallCommand() = default;

void WebAppUninstallCommand::Start() {
  if (!registrar_->GetAppById(app_id_)) {
    Abort(webapps::UninstallResultCode::kNoAppToUninstall);
    return;
  }

  DCHECK(state_ == State::kNotStarted);
  state_ = State::kPendingDataDeletion;

  // Note: It is supported to re-start an uninstall on startup, so
  // `is_uninstalling()` is not checked. It is a class invariant that there can
  // never be more than one uninstall task operating on the same web app at the
  // same time.
  {
    ScopedRegistryUpdate update(sync_bridge_);
    WebApp* app = update->UpdateApp(app_id_);
    DCHECK(app);
    app->SetIsUninstalling(true);
  }
  install_manager_->NotifyWebAppWillBeUninstalled(app_id_);

  RemoveAppIsolationState(profile_prefs_, app_origin_);

  // Uninstall any sub-apps the app has.
  // TODO(phillis): Fix this command to get locks for all sub-app ids as well.
  // https://crbug.com/1341337
  std::vector<AppId> sub_app_ids = registrar_->GetAllSubAppIds(app_id_);
  num_pending_sub_app_uninstalls_ = sub_app_ids.size();
  for (const AppId& sub_app_id : sub_app_ids) {
    if (registrar_->GetAppById(sub_app_id) == nullptr)
      continue;
    install_finalizer_->UninstallExternalWebApp(
        sub_app_id, WebAppManagement::Type::kSubApp,
        webapps::WebappUninstallSource::kSubApp,
        base::BindOnce(&WebAppUninstallCommand::OnSubAppUninstalled,
                       weak_factory_.GetWeakPtr()));
  }

  os_integration_manager_->UninstallAllOsHooks(
      app_id_, base::BindOnce(&WebAppUninstallCommand::OnOsHooksUninstalled,
                              weak_factory_.GetWeakPtr()));
  icon_manager_->DeleteData(
      app_id_, base::BindOnce(&WebAppUninstallCommand::OnIconDataDeleted,
                              weak_factory_.GetWeakPtr()));

  translation_manager_->DeleteTranslations(
      app_id_, base::BindOnce(&WebAppUninstallCommand::OnTranslationDataDeleted,
                              weak_factory_.GetWeakPtr()));
}

void WebAppUninstallCommand::Abort(webapps::UninstallResultCode code) {
  if (!callback_)
    return;
  SignalCompletionAndSelfDestruct(CommandResult::kFailure,
                                  base::BindOnce(std::move(callback_), code));
}

void WebAppUninstallCommand::OnSubAppUninstalled(
    webapps::UninstallResultCode code) {
  errors_ = errors_ || (code != webapps::UninstallResultCode::kSuccess);
  num_pending_sub_app_uninstalls_--;
  DCHECK_GE(num_pending_sub_app_uninstalls_, 0u);
  MaybeFinishUninstall();
}

void WebAppUninstallCommand::OnOsHooksUninstalled(OsHooksErrors errors) {
  DCHECK(state_ == State::kPendingDataDeletion);
  hooks_uninstalled_ = true;
  // TODO(https://crbug.com/1293234): Remove after flakiness is solved.
  DLOG_IF(ERROR, errors.any())
      << "OS integration errors for " << app_id_ << ": " << errors.to_string();
  base::UmaHistogramBoolean("WebApp.Uninstall.OsHookSuccess", errors.none());
  errors_ = errors_ || errors.any();
  MaybeFinishUninstall();
}

void WebAppUninstallCommand::OnIconDataDeleted(bool success) {
  DCHECK(state_ == State::kPendingDataDeletion);
  app_data_deleted_ = true;
  // TODO(https://crbug.com/1293234): Remove after flakiness is solved.
  DLOG_IF(ERROR, !success) << "Error deleting icon data for " << app_id_;
  base::UmaHistogramBoolean("WebApp.Uninstall.IconDataSuccess", success);
  errors_ = errors_ || !success;
  MaybeFinishUninstall();
}

void WebAppUninstallCommand::OnTranslationDataDeleted(bool success) {
  DCHECK(state_ == State::kPendingDataDeletion);
  translation_data_deleted_ = true;
  errors_ = errors_ || !success;
  MaybeFinishUninstall();
}

void WebAppUninstallCommand::MaybeFinishUninstall() {
  DCHECK(state_ == State::kPendingDataDeletion);
  if (!hooks_uninstalled_ || !app_data_deleted_ ||
      num_pending_sub_app_uninstalls_ > 0 || !translation_data_deleted_) {
    return;
  }
  DCHECK_EQ(num_pending_sub_app_uninstalls_, 0u);
  state_ = State::kDone;

  base::UmaHistogramBoolean("WebApp.Uninstall.Result", !errors_);

  webapps::InstallableMetrics::TrackUninstallEvent(source_);
  {
    DCHECK_NE(registrar_->GetAppById(app_id_), nullptr);
    ScopedRegistryUpdate update(sync_bridge_);
    update->DeleteApp(app_id_);
  }
  install_manager_->NotifyWebAppUninstalled(app_id_);

  SignalCompletionAndSelfDestruct(
      errors_ ? CommandResult::kFailure : CommandResult::kSuccess,
      base::BindOnce(std::move(callback_),
                     errors_ ? webapps::UninstallResultCode::kError
                             : webapps::UninstallResultCode::kSuccess));
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

base::Value WebAppUninstallCommand::ToDebugValue() const {
  return base::Value(base::StringPrintf(
      "WebAppUninstallCommand %d, app_id_: %s", id(), app_id_.c_str()));
}
}  // namespace web_app
