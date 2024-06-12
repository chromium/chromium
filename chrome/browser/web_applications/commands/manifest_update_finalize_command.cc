// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_update_finalize_command.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

ManifestUpdateFinalizeCommand::ManifestUpdateFinalizeCommand(
    const GURL& url,
    const webapps::AppId& app_id,
    std::unique_ptr<WebAppInstallInfo> install_info,
    ManifestWriteCallback write_callback,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive)
    : WebAppCommand<AppLock,
                    const GURL&,
                    const webapps::AppId&,
                    ManifestUpdateResult>(
          "ManifestUpdateFinalizeCommand",
          AppLockDescription(app_id),
          std::move(write_callback),
          /*args_for_shutdown=*/
          std::make_tuple(url, app_id, ManifestUpdateResult::kSystemShutdown)),
      url_(url),
      app_id_(app_id),
      install_info_(std::move(install_info)),
      keep_alive_(std::move(keep_alive)),
      profile_keep_alive_(std::move(profile_keep_alive)) {
  CHECK(install_info_);
  GetMutableDebugValue().Set("url", url_.spec());
  GetMutableDebugValue().Set("app_id", app_id_);
  GetMutableDebugValue().Set("install_info_->manifest_id",
                             install_info_->manifest_id().spec());
  GetMutableDebugValue().Set("install_info_->start_url",
                             install_info_->start_url().spec());
}

ManifestUpdateFinalizeCommand::~ManifestUpdateFinalizeCommand() = default;

void ManifestUpdateFinalizeCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  // Preserve the user's choice of form factor to open the app with.
  install_info_->user_display_mode =
      lock_->registrar().GetAppUserDisplayMode(app_id_);

  // ManifestUpdateCheckCommand must have already done validation of origin
  // association data needed by this app and set validated_scope_extensions even
  // if it is empty.
  CHECK(install_info_->validated_scope_extensions.has_value());

  lock_->install_finalizer().FinalizeUpdate(
      *install_info_,
      base::BindOnce(&ManifestUpdateFinalizeCommand::OnInstallationComplete,
                     AsWeakPtr()));
}

void ManifestUpdateFinalizeCommand::OnInstallationComplete(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  if (!IsSuccess(code)) {
    CompleteCommand(code, ManifestUpdateResult::kAppUpdateFailed);
    return;
  }

  DCHECK_EQ(app_id_, app_id);
  DCHECK(!GetManifestDataChanges(
              *lock_->registrar().GetAppById(app_id_),
              /*existing_app_icon_bitmaps=*/nullptr,
              /*existing_shortcuts_menu_icon_bitmaps=*/nullptr, *install_info_)
              .other_fields_changed);
  DCHECK_EQ(code, webapps::InstallResultCode::kSuccessAlreadyInstalled);

  lock_->sync_bridge().SetAppManifestUpdateTime(app_id, base::Time::Now());
  CompleteCommand(code, ManifestUpdateResult::kAppUpdated);
}

void ManifestUpdateFinalizeCommand::CompleteCommand(
    webapps::InstallResultCode code,
    ManifestUpdateResult result) {
  GetMutableDebugValue().Set("installation_code", base::ToString(code));
  GetMutableDebugValue().Set("result", base::ToString(result));
  CompleteAndSelfDestruct(
      IsSuccess(code) ? CommandResult::kSuccess : CommandResult::kFailure, url_,
      app_id_, result);
}

}  // namespace web_app
