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

namespace web_app {

LaunchWebAppCommand::LaunchWebAppCommand(
    Profile* profile,
    WebAppProvider* provider,
    apps::AppLaunchParams params,
    LaunchWebAppWindowSetting launch_setting,
    LaunchWebAppCallback callback)
    : WebAppCommandTemplate<AppLock>("LaunchWebAppCommand"),
      params_(std::move(params)),
      launch_setting_(launch_setting),
      callback_(std::move(callback)),
      lock_description_(std::make_unique<AppLockDescription>(params_.app_id)),
      profile_(profile),
      provider_(provider) {
  CHECK(provider);
}

LaunchWebAppCommand::~LaunchWebAppCommand() = default;

void LaunchWebAppCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  provider_->ui_manager().WaitForFirstRunService(
      *profile_, base::BindOnce(&LaunchWebAppCommand::FirstRunServiceCompleted,
                                weak_factory_.GetWeakPtr()));
}

const LockDescription& LaunchWebAppCommand::lock_description() const {
  return *lock_description_;
}

base::Value LaunchWebAppCommand::ToDebugValue() const {
  return base::Value(debug_value_.Clone());
}

void LaunchWebAppCommand::OnShutdown() {
  Complete(CommandResult::kShutdown);
}

void LaunchWebAppCommand::FirstRunServiceCompleted(bool success) {
  debug_value_.Set("first_run_success", base::Value(success));
  if (!success) {
    Complete(CommandResult::kFailure);
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
  debug_value_.Set("launch_web_app_debug_value", std::move(debug_value));
  Complete(CommandResult::kSuccess, std::move(browser), std::move(web_contents),
           container);
}

void LaunchWebAppCommand::Complete(
    CommandResult result,
    base::WeakPtr<Browser> browser,
    base::WeakPtr<content::WebContents> web_contents,
    apps::LaunchContainer container) {
  SignalCompletionAndSelfDestruct(
      result,
      base::BindOnce(std::move(callback_), browser, web_contents, container));
}

}  // namespace web_app
