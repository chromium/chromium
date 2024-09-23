// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_from_info_command.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/jobs/install_from_info_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/web_app_uninstall_and_replace_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {
namespace {

webapps::AppId GetAppId(const WebAppInstallInfo& install_info) {
  return GenerateAppIdFromManifestId(install_info.manifest_id(),
                                     install_info.parent_app_manifest_id);
}
}  // namespace

InstallFromInfoCommand::InstallFromInfoCommand(
    Profile* profile,
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    OnceInstallCallback install_callback,
    std::optional<WebAppInstallParams> install_params)
    : WebAppCommand<AppLock, const webapps::AppId&, webapps::InstallResultCode>(
          "InstallFromInfoCommand",
          AppLockDescription(GetAppId(*install_info)),
          std::move(install_callback),
          /*args_for_shutdown=*/
          std::make_tuple(/*app_id=*/
                          GetAppId(*install_info),
                          webapps::InstallResultCode::
                              kCancelledOnWebAppProviderShuttingDown)),
      profile_(*profile),
      app_id_(GetAppId(*install_info)) {
  GetMutableDebugValue().Set("manifest_id", install_info->manifest_id().spec());
  GetMutableDebugValue().Set("app_id", app_id_);
  install_from_info_job_ = std::make_unique<InstallFromInfoJob>(
      profile, *GetMutableDebugValue().EnsureDict("install_from_info_job"),
      std::move(install_info), overwrite_existing_manifest_fields,
      install_surface, install_params,
      base::BindOnce(&InstallFromInfoCommand::OnInstallFromInfoJobCompleted,
                     weak_factory_.GetWeakPtr()));
}

InstallFromInfoCommand::~InstallFromInfoCommand() = default;

void InstallFromInfoCommand::OnShutdown(
    base::PassKey<WebAppCommandManager>) const {
  webapps::InstallableMetrics::TrackInstallResult(
      false, install_from_info_job_->install_surface());
}

void InstallFromInfoCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);
  install_from_info_job_->Start(lock_.get());
}

void InstallFromInfoCommand::OnInstallFromInfoJobCompleted(
    webapps::AppId app_id,
    webapps::InstallResultCode code) {
  bool was_install_success = webapps::IsSuccess(code);
  if (!was_install_success) {
    CompleteAndSelfDestruct(CommandResult::kFailure, app_id_, code);
    return;
  }

  webapps::InstallableMetrics::TrackInstallResult(
      was_install_success, install_from_info_job_->install_surface());
  CompleteAndSelfDestruct(CommandResult::kSuccess, app_id_, code);
}

}  // namespace web_app
