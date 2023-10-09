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
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

InstallFromInfoCommand::InstallFromInfoCommand(
    Profile* profile,
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    OnceInstallCallback install_callback)
    : WebAppCommandTemplate<AppLock>("InstallFromInfoCommand"),
      profile_(*profile),
      manifest_id_(
          install_info->manifest_id.is_empty()
              ? GenerateManifestIdFromStartUrlOnly(install_info->start_url)
              : install_info->manifest_id),
      app_id_(
          GenerateAppIdFromManifestId(manifest_id_,
                                      install_info->parent_app_manifest_id)),
      install_callback_(base::BindOnce(
          [](OnceInstallCallback install_callback,
             const webapps::AppId& app_id,
             webapps::InstallResultCode code,
             bool _) { std::move(install_callback).Run(app_id, code); },
          std::move(install_callback))),
      lock_description_(std::make_unique<AppLockDescription>(app_id_)) {
  install_from_info_job_ = std::make_unique<InstallFromInfoJob>(
      profile, std::move(install_info), overwrite_existing_manifest_fields,
      install_surface, /*install_params=*/absl::nullopt,
      base::BindOnce(&InstallFromInfoCommand::OnInstallFromInfoJobCompleted,
                     weak_factory_.GetWeakPtr()));
}

InstallFromInfoCommand::InstallFromInfoCommand(
    Profile* profile,
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    OnceInstallCallback install_callback,
    const WebAppInstallParams& install_params)
    : WebAppCommandTemplate<AppLock>("InstallFromInfoCommand"),
      profile_(*profile),
      manifest_id_(
          install_info->manifest_id.is_empty()
              ? GenerateManifestIdFromStartUrlOnly(install_info->start_url)
              : install_info->manifest_id),
      app_id_(
          GenerateAppIdFromManifestId(manifest_id_,
                                      install_info->parent_app_manifest_id)),
      install_callback_(base::BindOnce(
          [](OnceInstallCallback install_callback,
             const webapps::AppId& app_id,
             webapps::InstallResultCode code,
             bool _) { std::move(install_callback).Run(app_id, code); },
          std::move(install_callback))),
      lock_description_(std::make_unique<AppLockDescription>(app_id_)) {
  install_from_info_job_ = std::make_unique<InstallFromInfoJob>(
      profile, std::move(install_info), overwrite_existing_manifest_fields,
      install_surface, install_params,
      base::BindOnce(&InstallFromInfoCommand::OnInstallFromInfoJobCompleted,
                     weak_factory_.GetWeakPtr()));
}

InstallFromInfoCommand::InstallFromInfoCommand(
    Profile* profile,
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    InstallAndReplaceCallback install_callback,
    const WebAppInstallParams& install_params,
    const std::vector<webapps::AppId>& apps_or_extensions_to_uninstall)
    : WebAppCommandTemplate<AppLock>("InstallFromInfoCommand"),
      profile_(*profile),
      manifest_id_(
          install_info->manifest_id.is_empty()
              ? GenerateManifestIdFromStartUrlOnly(install_info->start_url)
              : install_info->manifest_id),
      app_id_(
          GenerateAppIdFromManifestId(manifest_id_,
                                      install_info->parent_app_manifest_id)),
      install_callback_(std::move(install_callback)),
      apps_or_extensions_to_uninstall_(apps_or_extensions_to_uninstall),
      lock_description_(std::make_unique<AppLockDescription>(app_id_)) {
  install_from_info_job_ = std::make_unique<InstallFromInfoJob>(
      profile, std::move(install_info), overwrite_existing_manifest_fields,
      install_surface, install_params,
      base::BindOnce(&InstallFromInfoCommand::OnInstallFromInfoJobCompleted,
                     weak_factory_.GetWeakPtr()));
}
InstallFromInfoCommand::~InstallFromInfoCommand() = default;

const LockDescription& InstallFromInfoCommand::lock_description() const {
  return *lock_description_;
}

void InstallFromInfoCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);
  install_from_info_job_->Start(lock_.get());
}

void InstallFromInfoCommand::OnShutdown() {
  Abort(webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
  return;
}

base::Value InstallFromInfoCommand::ToDebugValue() const {
  base::Value::Dict dict;
  dict.Set("install_from_info_job", install_from_info_job_
                                        ? install_from_info_job_->ToDebugValue()
                                        : base::Value());
  dict.Set("uninstall_and_replace_job",
           uninstall_and_replace_job_
               ? uninstall_and_replace_job_->ToDebugValue()
               : base::Value());
  return base::Value(std::move(dict));
}

void InstallFromInfoCommand::OnInstallFromInfoJobCompleted(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hook_errors) {
  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code));
  if (!webapps::IsSuccess(code)) {
    Abort(code);
    return;
  }

  uninstall_and_replace_job_.emplace(
      &profile_.get(), *lock_, std::move(apps_or_extensions_to_uninstall_),
      app_id,
      base::BindOnce(&InstallFromInfoCommand::OnUninstallAndReplaced,
                     weak_factory_.GetWeakPtr(), std::move(code)));
  uninstall_and_replace_job_->Start();
}

void InstallFromInfoCommand::OnUninstallAndReplaced(
    webapps::InstallResultCode code,
    bool did_uninstall_and_replace) {
  if (!install_callback_) {
    return;
  }

  SignalCompletionAndSelfDestruct(
      webapps::IsSuccess(code) ? CommandResult::kSuccess
                               : CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), std::move(app_id_), code,
                     did_uninstall_and_replace));
}

void InstallFromInfoCommand::Abort(webapps::InstallResultCode code) {
  webapps::InstallableMetrics::TrackInstallResult(false);
  if (!install_callback_) {
    return;
  }

  SignalCompletionAndSelfDestruct(
      (code ==
       webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown)
          ? CommandResult::kShutdown
          : CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), std::move(app_id_), code,
                     /*did_uninstall_and_replace=*/false));
}

}  // namespace web_app
