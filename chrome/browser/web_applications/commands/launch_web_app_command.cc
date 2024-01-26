// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/launch_web_app_command.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace web_app {

LaunchWebAppCommand::LaunchWebAppCommand(
    Profile* profile,
    WebAppProvider* provider,
    apps::AppLaunchParams params,
    LaunchWebAppWindowSetting launch_setting,
    LaunchWebAppCallback callback)
    : WebAppCommand<AppLock,
                    base::WeakPtr<Browser>,
                    base::WeakPtr<content::WebContents>,
                    apps::LaunchContainer>(
          "LaunchWebAppCommand",
          AppLockDescription(params.app_id),
          std::move(callback),
          /*args_for_shutdown=*/
          std::make_tuple(nullptr,
                          nullptr,
                          apps::LaunchContainer::kLaunchContainerNone)),
      params_(std::move(params)),
      launch_setting_(launch_setting),
      profile_(*profile),
      provider_(*provider) {
  CHECK(provider);
}

LaunchWebAppCommand::~LaunchWebAppCommand() = default;

void LaunchWebAppCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  provider_->ui_manager().WaitForFirstRunService(
      *profile_, base::BindOnce(&LaunchWebAppCommand::FirstRunServiceCompleted,
                                weak_factory_.GetWeakPtr()));
}

void LaunchWebAppCommand::FirstRunServiceCompleted(bool success) {
  GetMutableDebugValue().Set("first_run_success", base::Value(success));
  if (!success) {
    CompleteAndSelfDestruct(CommandResult::kFailure, nullptr, nullptr,
                            apps::LaunchContainer::kLaunchContainerNone);
    return;
  }

  provider_->ui_manager().LaunchWebApp(
      std::move(params_), launch_setting_, *profile_,
      base::BindOnce(&LaunchWebAppCommand::OnAppLaunched,
                     weak_factory_.GetWeakPtr()),
      *lock_);
}

void LaunchWebAppCommand::OnAppLaunched(
    base::WeakPtr<Browser> browser,
    base::WeakPtr<content::WebContents> web_contents,
    apps::LaunchContainer container,
    base::Value debug_value) {
  GetMutableDebugValue().Set("launch_web_app_debug_value",
                             std::move(debug_value));
  CompleteAndSelfDestruct(CommandResult::kSuccess, std::move(browser),
                          std::move(web_contents), container);
}

}  // namespace web_app
