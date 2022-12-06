// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_from_info_command.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace web_app {

InstallFromInfoCommand::InstallFromInfoCommand(
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    OnceInstallCallback install_callback)
    : WebAppCommandTemplate<AppLock>("InstallFromInfoCommand"),
      lock_description_(
          std::make_unique<AppLockDescription, base::flat_set<AppId>>(
              {GenerateAppId(install_info->manifest_id,
                             install_info->start_url)})),
      app_id_(
          GenerateAppId(install_info->manifest_id, install_info->start_url)),
      install_info_(std::move(install_info)),
      overwrite_existing_manifest_fields_(overwrite_existing_manifest_fields),
      install_surface_(install_surface),
      install_callback_(std::move(install_callback)) {
  PopulateInitialDebugInfo();
}

InstallFromInfoCommand::InstallFromInfoCommand(
    std::unique_ptr<WebAppInstallInfo> install_info,
    bool overwrite_existing_manifest_fields,
    webapps::WebappInstallSource install_surface,
    OnceInstallCallback install_callback,
    const WebAppInstallParams& install_params)
    : WebAppCommandTemplate<AppLock>("InstallFromInfoCommand"),
      lock_description_(
          std::make_unique<AppLockDescription, base::flat_set<AppId>>(
              {GenerateAppId(install_info->manifest_id,
                             install_info->start_url)})),
      app_id_(
          GenerateAppId(install_info->manifest_id, install_info->start_url)),
      install_info_(std::move(install_info)),
      overwrite_existing_manifest_fields_(overwrite_existing_manifest_fields),
      install_surface_(install_surface),
      install_callback_(std::move(install_callback)),
      install_params_(install_params) {
  if (!install_params.locally_installed) {
    DCHECK(!install_params.add_to_applications_menu);
    DCHECK(!install_params.add_to_desktop);
    DCHECK(!install_params.add_to_quick_launch_bar);
  }
  DCHECK(install_info_->start_url.is_valid());
  PopulateInitialDebugInfo();
}
InstallFromInfoCommand::~InstallFromInfoCommand() = default;

LockDescription& InstallFromInfoCommand::lock_description() const {
  return *lock_description_;
}

void InstallFromInfoCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  PopulateProductIcons(install_info_.get(),
                       /*icons_map=*/nullptr);
  // No IconsMap to populate shortcut item icons from.

  if (install_params_) {
    ApplyParamsToWebAppInstallInfo(*install_params_, *install_info_);
  }

  if (webapps::InstallableMetrics::IsReportableInstallSource(
          install_surface_)) {
    webapps::InstallableMetrics::TrackInstallEvent(install_surface_);
  }

  WebAppInstallFinalizer::FinalizeOptions options(install_surface_);
  options.locally_installed = true;
  options.overwrite_existing_manifest_fields =
      overwrite_existing_manifest_fields_;

  if (install_params_) {
    ApplyParamsToFinalizeOptions(*install_params_, options);
  } else {
    options.bypass_os_hooks = true;
  }

  lock_->install_finalizer().FinalizeInstall(
      *install_info_, options,
      base::BindOnce(&InstallFromInfoCommand::OnInstallCompleted,
                     weak_factory_.GetWeakPtr()));
}

void InstallFromInfoCommand::PopulateInitialDebugInfo() {
  debug_value_.Set("app_id", app_id_);
  debug_value_.Set("start_url", install_info_->start_url.spec());
  debug_value_.Set("overwrite_existing_manifest_fields",
                   overwrite_existing_manifest_fields_);
  debug_value_.Set("install_surface", static_cast<int>(install_surface_));
  debug_value_.Set("has_install_params", install_params_ ? true : false);
}

void InstallFromInfoCommand::Abort(webapps::InstallResultCode code) {
  debug_value_.Set("result_code", base::StreamableToString(code));
  if (!install_callback_)
    return;
  webapps::InstallableMetrics::TrackInstallResult(false);
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), app_id_, code));
}

void InstallFromInfoCommand::OnInstallCompleted(const AppId& app_id,
                                                webapps::InstallResultCode code,
                                                OsHooksErrors os_hooks_errors) {
  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code));
  debug_value_.Set("result_code", base::StreamableToString(code));
  SignalCompletionAndSelfDestruct(
      webapps::IsSuccess(code) ? CommandResult::kSuccess
                               : CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), app_id, code));
}

void InstallFromInfoCommand::OnSyncSourceRemoved() {}

void InstallFromInfoCommand::OnShutdown() {
  Abort(webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
  return;
}

base::Value InstallFromInfoCommand::ToDebugValue() const {
  return base::Value(debug_value_.Clone());
}

}  // namespace web_app
