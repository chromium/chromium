// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/launch_or_reparent_web_contents_into_app_command.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

LaunchOrReparentWebContentsIntoAppCommand::
    LaunchOrReparentWebContentsIntoAppCommand(
        const webapps::AppId& app_id,
        base::WeakPtr<content::WebContents> web_contents,
        base::OnceCallback<void(LaunchOrReparentResult)> callback)
    : WebAppCommand<AppLock, LaunchOrReparentResult>(
          "LaunchOrReparentWebContentsIntoAppCommand",
          AppLockDescription(app_id),
          base::BindOnce([](LaunchOrReparentResult result) {
            base::UmaHistogramEnumeration(
                "WebApp.Command.LaunchOrReparentResult", result);
            return result;
          }).Then(std::move(callback)),
          /*args_for_shutdown=*/
          std::make_tuple(LaunchOrReparentResult::kShutdown)),
      app_id_(app_id),
      web_contents_(web_contents) {}

LaunchOrReparentWebContentsIntoAppCommand::
    ~LaunchOrReparentWebContentsIntoAppCommand() = default;

void LaunchOrReparentWebContentsIntoAppCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  GetMutableDebugValue().Set("app_id", app_id_);

  if (!web_contents_) {
    CompleteAndSelfDestruct(CommandResult::kSuccess,
                            LaunchOrReparentResult::kWebContentsGone);
    return;
  }

  if (!lock_->registrar().AppMatches(app_id_,
                                     WebAppFilter::OpensInDedicatedWindow())) {
    CompleteAndSelfDestruct(
        CommandResult::kSuccess,
        LaunchOrReparentResult::kAppNotInstalledAsDedicatedWindow);
    return;
  }

  CHECK_NE(lock_->registrar().GetAppEffectiveDisplayMode(app_id_),
           blink::mojom::DisplayMode::kBrowser);

  std::optional<webapps::AppId> current_app_id =
      lock_->web_contents_manager().GetAppIdForWebContents(web_contents_.get());
  GetMutableDebugValue().Set("web_contents_app_id",
                             current_app_id ? *current_app_id : "none");

  bool in_scope = current_app_id && (*current_app_id == app_id_);
  GetMutableDebugValue().Set("in_scope", in_scope);

  if (in_scope) {
    bool can_reparent = lock_->ui_manager().CanReparentAppTabToWindow(
        app_id_, /*shortcut_created=*/true, web_contents_.get());
    GetMutableDebugValue().Set("can_reparent", can_reparent);

    if (can_reparent) {
      lock_->ui_manager().ReparentAppTabToWindow(web_contents_.get(), app_id_,
                                                 /*shortcut_created=*/true);
      CompleteAndSelfDestruct(CommandResult::kSuccess,
                              LaunchOrReparentResult::kReparented);
      return;
    }
  }

  // Otherwise launch
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());

  apps::AppLaunchParams params =
      WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
          app_id_, *base::CommandLine::ForCurrentProcess(),
          /*current_directory=*/base::FilePath(),
          /*protocol_handler_launch_url=*/std::nullopt,
          /*file_launch_url=*/std::nullopt, /*launch_files=*/{});

  lock_->ui_manager().LaunchWebApp(
      std::move(params), LaunchWebAppWindowSetting::kOverrideWithWebAppConfig,
      *profile,
      base::BindOnce(&LaunchOrReparentWebContentsIntoAppCommand::OnAppLaunched,
                     weak_factory_.GetWeakPtr()),
      *lock_);
}

void LaunchOrReparentWebContentsIntoAppCommand::OnAppLaunched(
    base::WeakPtr<Browser> browser,
    base::WeakPtr<content::WebContents> web_contents,
    apps::LaunchContainer container,
    base::Value debug_value) {
  GetMutableDebugValue().Set("launch", std::move(debug_value));
  CompleteAndSelfDestruct(CommandResult::kSuccess,
                          LaunchOrReparentResult::kLaunched);
}

}  // namespace web_app
