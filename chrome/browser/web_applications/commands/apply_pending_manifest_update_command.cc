// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/apply_pending_manifest_update_command.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

namespace web_app {

std::ostream& operator<<(std::ostream& os,
                         ApplyPendingManifestUpdateResult stage) {
  switch (stage) {
    case ApplyPendingManifestUpdateResult::kSystemShutdown:
      return os << "kSystemShutdown";
    case ApplyPendingManifestUpdateResult::kAppNotInstalled:
      return os << "kAppNotInstalled";
    case ApplyPendingManifestUpdateResult::kIconChangeAppliedSuccessfully:
      return os << "kIconChangeAppliedSuccessfully";
    case ApplyPendingManifestUpdateResult::
        kFailedToOverwriteIconsFromPendingIcons:
      return os << "kFailedToOverwriteIconsFromPendingIcons";
    case ApplyPendingManifestUpdateResult::kNoPendingUpdate:
      return os << "kNoPendingUpdate";
    case ApplyPendingManifestUpdateResult::kFailedToRemovePendingIconsFromDisk:
      return os << "kFailedToRemovePendingIconsFromDisk";
  }
}

ApplyPendingManifestUpdateCommand::ApplyPendingManifestUpdateCommand(
    const webapps::AppId& app_id,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    CompletedCallback callback)
    : WebAppCommand<AppLock, ApplyPendingManifestUpdateResult>(
          "ApplyPendingManifestUpdateCommand",
          AppLockDescription(app_id),
          base::BindOnce([](ApplyPendingManifestUpdateResult result) {
            base::UmaHistogramEnumeration(
                "WebApp.Update.ApplyPendingManifestUpdateResult", result);
            return result;
          }).Then(std::move(callback)),
          /*args_for_shutdown=*/
          std::make_tuple(ApplyPendingManifestUpdateResult::kSystemShutdown)),
      app_id_(app_id),
      keep_alive_(std::move(keep_alive)),
      profile_keep_alive_(std::move(profile_keep_alive)) {
  GetMutableDebugValue().Set("app_id", app_id_);
}

ApplyPendingManifestUpdateCommand::~ApplyPendingManifestUpdateCommand() =
    default;

void ApplyPendingManifestUpdateCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);
  if (!lock_->registrar().IsInRegistrar(app_id_)) {
    CompleteCommandAndSelfDestruct(
        ApplyPendingManifestUpdateResult::kAppNotInstalled);
    return;
  }
  const WebApp* web_app = lock_->registrar().GetAppById(app_id_);

  if (!web_app->pending_update_info().has_value()) {
    CompleteCommandAndSelfDestruct(
        ApplyPendingManifestUpdateResult::kNoPendingUpdate);
    return;
  }
  const proto::PendingUpdateInfo& pending_update =
      web_app->pending_update_info().value();

  // TODO(crbug.com/444497489): Update the web app for app name changes.

  if (pending_update.trusted_icons().empty() &&
      pending_update.manifest_icons().empty()) {
    // TODO(crbug.com/444497489): Move on to synchronize and skip the
    // overwriting icons process.
    CompleteCommandAndSelfDestruct(
        ApplyPendingManifestUpdateResult::kAppNotInstalled);
    return;
  }

  lock_->icon_manager().OverwriteAppIconsFromPendingIcons(
      app_id_, PassKey(),
      base::BindOnce(
          &ApplyPendingManifestUpdateCommand::ApplyPendingIconToWebApp,
          AsWeakPtr()));
}

void ApplyPendingManifestUpdateCommand::ApplyPendingIconToWebApp(bool success) {
  // TODO(crbug.com/444497489): Update for icon changes to the web app.
  if (!success) {
    CompleteCommandAndSelfDestruct(ApplyPendingManifestUpdateResult::
                                       kFailedToOverwriteIconsFromPendingIcons);
    return;
  }

  // Deletion of pending manifest and pending trusted icon directories should
  // only happen after the web app has been updated and OS integration is
  // completed.
  lock_->icon_manager().DeletePendingIconData(
      app_id_, WebAppIconManager::DeletePendingPassKey(),
      base::BindOnce([](bool success) {
        return success ? ApplyPendingManifestUpdateResult::
                             kIconChangeAppliedSuccessfully
                       : ApplyPendingManifestUpdateResult::
                             kFailedToRemovePendingIconsFromDisk;
      })
          .Then(base::BindOnce(&ApplyPendingManifestUpdateCommand::
                                   CompleteCommandAndSelfDestruct,
                               AsWeakPtr())));
}

void ApplyPendingManifestUpdateCommand::CompleteCommandAndSelfDestruct(
    ApplyPendingManifestUpdateResult check_result) {
  GetMutableDebugValue().Set("result", base::ToString(check_result));

  CommandResult command_result;
  switch (check_result) {
    case ApplyPendingManifestUpdateResult::kAppNotInstalled:
    case ApplyPendingManifestUpdateResult::kIconChangeAppliedSuccessfully:
    case ApplyPendingManifestUpdateResult::
        kFailedToOverwriteIconsFromPendingIcons:
    case ApplyPendingManifestUpdateResult::kNoPendingUpdate:
    case ApplyPendingManifestUpdateResult::kFailedToRemovePendingIconsFromDisk:
      command_result = CommandResult::kSuccess;
      break;
    case ApplyPendingManifestUpdateResult::kSystemShutdown:
      NOTREACHED() << "The value should only be specified in the constructor "
                      "and never given to this method.";
  }

  CompleteAndSelfDestruct(command_result, check_result);
}

}  // namespace web_app
