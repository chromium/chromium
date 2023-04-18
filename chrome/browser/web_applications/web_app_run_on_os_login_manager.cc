// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/notification_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace web_app {

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kRunOnOsLoginNotificationId[] = "run_on_os_login";
const char kWebAppPolicyManagerNotifierId[] = "web_app_policy_notifier";
#endif

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

    // TODO(crbug.com/1091964): Implement Run on OS Login mode selection and
    // launch app appropriately
    apps::AppLaunchParams params(
        app_id, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromOsLogin);

    if (lock.registrar().GetAppEffectiveDisplayMode(app_id) ==
        blink::mojom::DisplayMode::kBrowser) {
      params.container = apps::LaunchContainer::kLaunchContainerTab;
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    }

    const std::string app_name = lock.registrar().GetAppShortName(app_id);

    auto callback =
        base::BindOnce(&WebAppRunOnOsLoginManager::OnAppLaunchedOnOsLogin,
                       weak_ptr_factory_.GetWeakPtr(), app_id, app_name);
    scheduler_->LaunchAppWithCustomParams(std::move(params),
                                          std::move(callback));
  }
}

void WebAppRunOnOsLoginManager::OnAppLaunchedOnOsLogin(
    AppId app_id,
    std::string app_name,
    base::WeakPtr<Browser> browser,
    base::WeakPtr<content::WebContents> web_contents,
    apps::LaunchContainer container) {
// TODO(crbug.com/1341247): Implement notification for lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const std::string notification_id =
      std::string(kRunOnOsLoginNotificationId) + "_" + app_name;
  auto notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      /*title=*/std::u16string(),
      l10n_util::GetStringFUTF16(IDS_RUN_ON_OS_LOGIN_ENABLED,
                                 base::UTF8ToUTF16(app_name)),
      /* display_source= */ std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kWebAppPolicyManagerNotifierId,
                                 ash::NotificationCatalogName::kWebAppSettings),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::RepeatingClosure()),
      gfx::kNoneIcon, message_center::SystemNotificationWarningLevel::NORMAL);
  SystemNotificationHelper::GetInstance()->Display(notification);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
