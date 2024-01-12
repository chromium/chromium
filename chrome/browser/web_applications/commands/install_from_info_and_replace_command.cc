// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_from_info_and_replace_command.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/jobs/install_from_info_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/web_app_uninstall_and_replace_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {
namespace {
webapps::ManifestId GetManifestIdWithBackup(
    const WebAppInstallInfo& install_info) {
  return install_info.manifest_id.is_empty()
             ? GenerateManifestIdFromStartUrlOnly(install_info.start_url)
             : install_info.manifest_id;
}

webapps::AppId GetAppIdWithBackup(const WebAppInstallInfo& install_info) {
  return GenerateAppIdFromManifestId(GetManifestIdWithBackup(install_info),
                                     install_info.parent_app_manifest_id);
}
}  // namespace

InstallFromInfoAndReplaceCommand::InstallFromInfoAndReplaceCommand(
    Profile* profile,
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    InstallAndReplaceCallback install_callback,
    const WebAppInstallParams& install_params,
    const std::vector<webapps::AppId>& apps_or_extensions_to_uninstall)
    : WebAppCommand<AppLock,
                    const webapps::AppId&,
                    webapps::InstallResultCode,
                    bool /*did_uninstall_and_replace*/>(
          "InstallFromInfoAndReplaceCommand",
          AppLockDescription(GetAppIdWithBackup(*install_info)),
          std::move(install_callback),
          /*args_for_shutdown=*/
          std::make_tuple(/*app_id=*/
                          GetAppIdWithBackup(*install_info),
                          webapps::InstallResultCode::
                              kCancelledOnWebAppProviderShuttingDown,
                          /*did_uninstall_and_replace=*/false)),
      profile_(*profile),
      manifest_id_(GetManifestIdWithBackup(*install_info)),
      app_id_(GetAppIdWithBackup(*install_info)),
      apps_or_extensions_to_uninstall_(apps_or_extensions_to_uninstall) {
  install_from_info_job_ = std::make_unique<InstallFromInfoJob>(
      profile, *GetMutableDebugValue().EnsureDict("install_from_info_job"),
      std::move(install_info), overwrite_existing_manifest_fields,
      install_surface, install_params,
      base::BindOnce(
          &InstallFromInfoAndReplaceCommand::OnInstallFromInfoJobCompleted,
          weak_factory_.GetWeakPtr()));
}
InstallFromInfoAndReplaceCommand::~InstallFromInfoAndReplaceCommand() = default;

void InstallFromInfoAndReplaceCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);
  install_from_info_job_->Start(lock_.get());
}

void InstallFromInfoAndReplaceCommand::OnInstallFromInfoJobCompleted(
    webapps::AppId app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hook_errors) {
  bool was_install_success = webapps::IsSuccess(code);
  if (!was_install_success) {
    Abort(code);
    return;
  }

  webapps::InstallableMetrics::TrackInstallResult(was_install_success);

  uninstall_and_replace_job_ = std::make_unique<WebAppUninstallAndReplaceJob>(
      &profile_.get(),
      *GetMutableDebugValue().EnsureDict("uninstall_and_replace_job"), *lock_,
      std::move(apps_or_extensions_to_uninstall_), app_id,
      base::BindOnce(&InstallFromInfoAndReplaceCommand::OnUninstallAndReplaced,
                     weak_factory_.GetWeakPtr(), code));
  uninstall_and_replace_job_->Start();
}

void InstallFromInfoAndReplaceCommand::OnUninstallAndReplaced(
    webapps::InstallResultCode code,
    bool did_uninstall_and_replace) {
  CompleteAndSelfDestruct(webapps::IsSuccess(code) ? CommandResult::kSuccess
                                                   : CommandResult::kFailure,
                          app_id_, code, did_uninstall_and_replace);
}

void InstallFromInfoAndReplaceCommand::Abort(webapps::InstallResultCode code) {
  webapps::InstallableMetrics::TrackInstallResult(false);
  CompleteAndSelfDestruct(CommandResult::kFailure, app_id_, code,
                          /*did_uninstall_and_replace=*/false);
}

}  // namespace web_app
