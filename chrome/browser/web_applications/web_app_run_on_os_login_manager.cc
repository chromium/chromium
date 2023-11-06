// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {
bool g_skip_startup_for_testing_ = false;
}

WebAppRunOnOsLoginManager::WebAppRunOnOsLoginManager(Profile* profile)
    : profile_(profile) {}
WebAppRunOnOsLoginManager::~WebAppRunOnOsLoginManager() = default;

void WebAppRunOnOsLoginManager::SetProvider(base::PassKey<WebAppProvider>,
                                            WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppRunOnOsLoginManager::Start() {
  if (g_skip_startup_for_testing_) {
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
    return;
  }

  provider_->scheduler().ScheduleCallbackWithLock<AllAppsLock>(
      "WebAppRunOnOsLoginManager::RunAppsOnOsLogin",
      std::make_unique<AllAppsLockDescription>(),
      base::BindOnce(&WebAppRunOnOsLoginManager::RunAppsOnOsLogin,
                     GetWeakPtr()));
}

void WebAppRunOnOsLoginManager::RunAppsOnOsLogin(AllAppsLock& lock) {
  std::vector<std::string> app_names;

  for (const webapps::AppId& app_id : lock.registrar().GetAppIds()) {
    if (!IsRunOnOsLoginModeEnabledForAutostart(
            lock.registrar().GetAppRunOnOsLoginMode(app_id).value)) {
      continue;
    }

    // In case of already opened/restored apps, we do not launch them again
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

    std::string app_name = lock.registrar().GetAppShortName(app_id);
    app_names.push_back(std::move(app_name));

    // Schedule launch here, show notification when the app window pops up.
    provider_->scheduler().LaunchAppWithCustomParams(
        std::move(params),
        base::BindOnce(
            [](base::WeakPtr<WebAppProvider> provider,
               base::WeakPtr<Profile> profile,
               std::vector<std::string> app_names,
               base::WeakPtr<Browser> browser,
               base::WeakPtr<content::WebContents> web_contents,
               apps::LaunchContainer container) {
              if (app_names.empty()) {
                return;
              }
              provider->ui_manager().DisplayRunOnOsLoginNotification(
                  app_names, std::move(profile));
            },
            provider_->AsWeakPtr(), profile_->GetWeakPtr(),
            std::move(app_names)));
  }

}

base::WeakPtr<WebAppRunOnOsLoginManager>
WebAppRunOnOsLoginManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// static
base::AutoReset<bool> WebAppRunOnOsLoginManager::SkipStartupForTesting() {
  return {&g_skip_startup_for_testing_, true};
}

void WebAppRunOnOsLoginManager::RunAppsOnOsLoginForTesting() {
  provider_->scheduler().ScheduleCallbackWithLock<AllAppsLock>(
      "WebAppRunOnOsLoginManager::RunAppsOnOsLogin",
      std::make_unique<AllAppsLockDescription>(),
      base::BindOnce(&WebAppRunOnOsLoginManager::RunAppsOnOsLogin,
                     GetWeakPtr()));
}

}  // namespace web_app
