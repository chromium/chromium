// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_update_finalize_command.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/browser/install_result_code.h"
#include "manifest_update_data_fetch_command.h"

namespace web_app {

ManifestUpdateFinalizeCommand::ManifestUpdateFinalizeCommand(
    const GURL& url,
    const AppId& app_id,
    WebAppInstallInfo install_info,
    bool app_identity_update_allowed,
    ManifestWriteCallback write_callback,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive)
    : WebAppCommandTemplate<AppLock>("ManifestUpdateFinalizeCommand"),
      lock_description_(std::make_unique<AppLockDescription>(app_id)),
      url_(url),
      app_id_(app_id),
      install_info_(std::move(install_info)),
      app_identity_update_allowed_(app_identity_update_allowed),
      write_callback_(std::move(write_callback)),
      keep_alive_(std::move(keep_alive)),
      profile_keep_alive_(std::move(profile_keep_alive)) {}

ManifestUpdateFinalizeCommand::~ManifestUpdateFinalizeCommand() = default;

const LockDescription& ManifestUpdateFinalizeCommand::lock_description() const {
  return *lock_description_;
}

void ManifestUpdateFinalizeCommand::OnShutdown() {
  CompleteCommand(webapps::InstallResultCode::kUpdateTaskFailed,
                  ManifestUpdateResult::kAppUpdateFailed);
}

base::Value ManifestUpdateFinalizeCommand::ToDebugValue() const {
  base::Value::Dict data = debug_log_.Clone();
  data.Set("url", url_.spec());
  data.Set("app_id", app_id_);
  data.Set("stage", base::StreamableToString(stage_));
  return base::Value(std::move(data));
}

void ManifestUpdateFinalizeCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  DCHECK_EQ(stage_, ManifestUpdateStage::kAppWindowsClosed);

  lock_ = std::move(lock);

  if (!AllowUnpromptedNameUpdate(app_id_, lock_->registrar()) &&
      !app_identity_update_allowed_) {
    // The app's name must not change due to an automatic update, except for
    // default installed apps (that have been vetted).
    install_info_.title =
        base::UTF8ToUTF16(lock_->registrar().GetAppShortName(app_id_));
  }

  // Preserve the user's choice of form factor to open the app with.
  install_info_.user_display_mode =
      lock_->registrar().GetAppUserDisplayMode(app_id_);
  stage_ = ManifestUpdateStage::kPendingFinalizerUpdate;
  lock_->install_finalizer().FinalizeUpdate(
      install_info_,
      base::BindOnce(&ManifestUpdateFinalizeCommand::OnInstallationComplete,
                     AsWeakPtr()));
}

void ManifestUpdateFinalizeCommand::OnInstallationComplete(
    const AppId& app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hooks_errors) {
  DCHECK_EQ(stage_, ManifestUpdateStage::kPendingFinalizerUpdate);

  if (!IsSuccess(code)) {
    CompleteCommand(code, ManifestUpdateResult::kAppUpdateFailed);
    return;
  }

  DCHECK_EQ(app_id_, app_id);
  DCHECK(
      !IsUpdateNeededForManifest(app_id_, install_info_, lock_->registrar()));
  DCHECK_EQ(code, webapps::InstallResultCode::kSuccessAlreadyInstalled);

  lock_->sync_bridge().SetAppManifestUpdateTime(app_id, base::Time::Now());
  CompleteCommand(code, ManifestUpdateResult::kAppUpdated);
}

void ManifestUpdateFinalizeCommand::CompleteCommand(
    webapps::InstallResultCode code,
    ManifestUpdateResult result) {
  debug_log_.Set("installation_code", base::StreamableToString(code));
  debug_log_.Set("result", base::StreamableToString(result));
  SignalCompletionAndSelfDestruct(
      IsSuccess(code) ? CommandResult::kSuccess : CommandResult::kFailure,
      base::BindOnce(std::move(write_callback_), url_, app_id_, result));
}

}  // namespace web_app
