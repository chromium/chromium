// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/rewrite_diy_icons_command.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/mac/apps_folder_support.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_creator.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

namespace {
RewriteIconResult RecordIconMigrationResult(RewriteIconResult result) {
  base::UmaHistogramEnumeration("WebApp.DIY.IconMigrationResult", result);
  return result;
}
}  // namespace

RewriteDiyIconsCommand::RewriteDiyIconsCommand(
    const webapps::AppId& app_id,
    base::OnceCallback<void(RewriteIconResult)> callback)
    : WebAppCommand(
          "RewriteDiyIconsCommand",
          AppLockDescription({app_id}),
          base::BindOnce(&RecordIconMigrationResult).Then(std::move(callback)),
          std::make_tuple(RewriteIconResult::kUpdateShortcutFailed)),
      app_id_(app_id) {
  GetMutableDebugValue().Set("app_id", app_id_);
}

RewriteDiyIconsCommand::~RewriteDiyIconsCommand() = default;

void RewriteDiyIconsCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  const WebAppRegistrar& registrar = lock_->registrar();
  if (!registrar.AppMatches(app_id_, WebAppFilter::IsDiyWithOsShortcut())) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            RewriteIconResult::kUnexpectedAppStateChange);
    return;
  }

  if (registrar.IsDiyAppIconsMarkedMaskedOnMac(app_id_)) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            RewriteIconResult::kUnexpectedAppStateChange);
    return;
  }

  lock_->os_integration_manager().GetShortcutInfoForAppFromRegistrar(
      app_id_, base::BindOnce(&RewriteDiyIconsCommand::OnGetShortcutInfo,
                              weak_factory_.GetWeakPtr()));
}

void RewriteDiyIconsCommand::OnGetShortcutInfo(
    std::unique_ptr<ShortcutInfo> info) {
  if (!info) {
    CompleteAndSelfDestruct(CommandResult::kFailure,
                            RewriteIconResult::kShortcutInfoFetchFailed);
    return;
  }
  internals::PostShortcutIOTask(
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             bool use_ad_hoc_signing_for_web_app_shims,
             const ShortcutInfo& info) {
            WebAppShortcutCreator shortcut_creator(
                internals::GetShortcutDataDir(info), GetChromeAppsFolder(),
                &info, use_ad_hoc_signing_for_web_app_shims);
            std::vector<base::FilePath> updated_app_paths;
            std::move(callback).Run(shortcut_creator.UpdateShortcuts(
                /*create_if_needed=*/false, &updated_app_paths));
          },
          base::BindPostTaskToCurrentDefault(
              base::BindOnce(&RewriteDiyIconsCommand::OnUpdatedShortcuts,
                             weak_factory_.GetWeakPtr())),
          web_app::UseAdHocSigningForWebAppShims()),
      std::move(info));
}

void RewriteDiyIconsCommand::OnUpdatedShortcuts(bool success) {
  if (!success) {
    CompleteAndSelfDestruct(CommandResult::kFailure,
                            RewriteIconResult::kUpdateShortcutFailed);
    return;
  }
  auto update = lock_->sync_bridge().BeginUpdate();
  WebApp* web_app = update->UpdateApp(app_id_);
  CHECK(web_app);
  web_app->SetDiyAppIconsMaskedOnMac(true);

  CompleteAndSelfDestruct(CommandResult::kSuccess,
                          RewriteIconResult::kUpdateSucceeded);
}

}  // namespace web_app
