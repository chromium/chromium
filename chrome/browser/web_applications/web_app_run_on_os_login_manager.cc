// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

WebAppRunOnOsLoginManager::WebAppRunOnOsLoginManager(
    WebAppCommandScheduler* scheduler)
    : scheduler_(*scheduler) {
  DCHECK(scheduler);
}
WebAppRunOnOsLoginManager::~WebAppRunOnOsLoginManager() = default;

void WebAppRunOnOsLoginManager::Start() {
  if (skip_startup_for_testing_) {
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
    return;
  }

  scheduler_->ScheduleCallbackWithLock<AllAppsLock>(
      "WebAppRunOnOsLoginManager::RunAppsOnOsLogin",
      std::make_unique<AllAppsLockDescription>(),
      base::BindOnce(&WebAppRunOnOsLoginManager::RunAppsOnOsLogin,
                     GetWeakPtr()));
}

void WebAppRunOnOsLoginManager::RunAppsOnOsLogin(AllAppsLock& lock) {
  // With a full system lock acquired, getting all apps is safe and no filtering
  // of uninstalling apps etc. is required
  for (const AppId& app_id : lock.registrar().GetAppIds()) {
    if (lock.registrar().GetAppRunOnOsLoginMode(app_id).value ==
        RunOnOsLoginMode::kNotRun) {
      continue;
    }

    // In case of already opened/restored apps, we do not launch them again.
    if (lock.ui_manager().GetNumWindowsForApp(app_id) > 0) {
      continue;
    }

    // TODO(crbug.com/1091964): Implement Run on OS Login mode selection and
    // launch app appropriately.
    // For ROOL on ChromeOS, we only have managed web apps which need to be run
    // as standalone windows, never as tabs
    apps::AppLaunchParams params(
        app_id, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromOsLogin);

    scheduler_->LaunchAppWithCustomParams(std::move(params), base::DoNothing());
  }
}

base::WeakPtr<WebAppRunOnOsLoginManager>
WebAppRunOnOsLoginManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebAppRunOnOsLoginManager::SetSkipStartupForTesting(bool skip_startup) {
  skip_startup_for_testing_ = skip_startup;  // IN-TEST
}

void WebAppRunOnOsLoginManager::RunAppsOnOsLoginForTesting() {
  scheduler_->ScheduleCallbackWithLock<AllAppsLock>(
      "WebAppRunOnOsLoginManager::RunAppsOnOsLogin",
      std::make_unique<AllAppsLockDescription>(),
      base::BindOnce(&WebAppRunOnOsLoginManager::RunAppsOnOsLogin,
                     GetWeakPtr()));
}

}  // namespace web_app
