// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_url_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"

namespace web_app {

// static
std::unique_ptr<WebAppUninstallCommand>
WebAppUninstallCommand::CreateForRemoveInstallUrl(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    std::optional<webapps::AppId> app_id,
    WebAppManagement::Type install_source,
    GURL install_url,
    UninstallJob::Callback callback) {
  return base::WrapUnique(new WebAppUninstallCommand(
      uninstall_source, profile, app_id, install_source, install_url,
      std::move(callback)));
}

// static
std::unique_ptr<WebAppUninstallCommand>
WebAppUninstallCommand::CreateForRemoveInstallManagements(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    webapps::AppId app_id,
    WebAppManagementTypes install_sources,
    UninstallJob::Callback callback) {
  return base::WrapUnique(new WebAppUninstallCommand(
      uninstall_source, profile, app_id, install_sources, std::move(callback)));
}

// static
std::unique_ptr<WebAppUninstallCommand>
WebAppUninstallCommand::CreateForRemoveUserUninstallableManagement(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    webapps::AppId app_id,
    UninstallJob::Callback callback) {
  return base::WrapUnique(new WebAppUninstallCommand(
      uninstall_source, profile, app_id, std::move(callback)));
}

WebAppUninstallCommand::WebAppUninstallCommand(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    std::optional<webapps::AppId> app_id,
    WebAppManagement::Type install_source,
    GURL install_url,
    UninstallJob::Callback callback)
    : WebAppCommand<AllAppsLock, webapps::UninstallResultCode>(
          "WebAppUninstallCommand",
          AllAppsLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/webapps::UninstallResultCode::kShutdown),
      job_(std::make_unique<RemoveInstallUrlJob>(
          uninstall_source,
          profile,
          *GetMutableDebugValue().EnsureDict("remove_install_url_job"),
          app_id,
          install_source,
          install_url)) {}

WebAppUninstallCommand::WebAppUninstallCommand(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    webapps::AppId app_id,
    WebAppManagementTypes install_managements,
    UninstallJob::Callback callback)
    : WebAppCommand<AllAppsLock, webapps::UninstallResultCode>(
          "WebAppUninstallCommand",
          AllAppsLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/webapps::UninstallResultCode::kShutdown),
      job_(std::make_unique<RemoveInstallSourceJob>(
          uninstall_source,
          profile,
          *GetMutableDebugValue().EnsureDict("remove_install_source_job"),
          app_id,
          install_managements)) {}

WebAppUninstallCommand::WebAppUninstallCommand(
    webapps::WebappUninstallSource uninstall_source,
    Profile& profile,
    webapps::AppId app_id,
    UninstallJob::Callback callback)
    : WebAppCommand<AllAppsLock, webapps::UninstallResultCode>(
          "WebAppUninstallCommand",
          AllAppsLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/webapps::UninstallResultCode::kShutdown),
      job_(std::make_unique<RemoveInstallSourceJob>(
          uninstall_source,
          profile,
          *GetMutableDebugValue().EnsureDict("remove_web_app_job"),
          app_id,
          kUserUninstallableSources)) {
  CHECK(webapps::IsUserUninstall(uninstall_source))
      << "The uninstall source for removing all user-installable install "
         "management types must be a user uninstall source. Source:"
      << uninstall_source;
}

WebAppUninstallCommand::~WebAppUninstallCommand() = default;

void WebAppUninstallCommand::OnShutdown(
    base::PassKey<WebAppCommandManager>) const {
  base::UmaHistogramBoolean("WebApp.Uninstall.Result", false);
}

void WebAppUninstallCommand::StartWithLock(std::unique_ptr<AllAppsLock> lock) {
  lock_ = std::move(lock);
  job_->Start(*lock_, base::BindOnce(&WebAppUninstallCommand::OnCompletion,
                                     weak_factory_.GetWeakPtr()));
}

void WebAppUninstallCommand::OnCompletion(webapps::UninstallResultCode code) {
  base::UmaHistogramBoolean("WebApp.Uninstall.Result",
                            UninstallSucceeded(code));
  CompleteAndSelfDestruct(
      [code]() {
        switch (code) {
          case webapps::UninstallResultCode::kAppRemoved:
          case webapps::UninstallResultCode::kInstallSourceRemoved:
          case webapps::UninstallResultCode::kInstallUrlRemoved:
          case webapps::UninstallResultCode::kNoAppToUninstall:
            return CommandResult::kSuccess;
          case webapps::UninstallResultCode::kCancelled:
          case webapps::UninstallResultCode::kError:
            return CommandResult::kFailure;
          case webapps::UninstallResultCode::kShutdown:
            NOTREACHED();
        }
      }(),
      code);
}

}  // namespace web_app
