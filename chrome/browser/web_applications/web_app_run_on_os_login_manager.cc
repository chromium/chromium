// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "chrome/browser/apps/app_service/app_launch_params.h"
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
    WebAppRegistrar* app_registrar,
    WebAppCommandScheduler* scheduler)
    : app_registrar_(*app_registrar), scheduler_(*scheduler) {
  DCHECK(app_registrar);
  DCHECK(scheduler);
}
WebAppRunOnOsLoginManager::~WebAppRunOnOsLoginManager() = default;

void WebAppRunOnOsLoginManager::Start() {
  if (skip_startup_for_testing_) {
    return;
  }
  RunAppsOnOsLogin();
}

void WebAppRunOnOsLoginManager::RunAppsOnOsLogin() {
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
    return;
  }

  // It is safe to use the app ids because we are guaranteed to have all policy
  // force installs completed AND the policy settings have been saved locally.
  // It's not perfectly safe, and thus we filter out uninstalling apps, etc.
  for (const AppId& app_id : app_registrar_->GetAppIds()) {
    if (app_registrar_->IsUninstalling(app_id)) {
      continue;
    }

    if (app_registrar_->GetAppRunOnOsLoginMode(app_id).value ==
        RunOnOsLoginMode::kNotRun) {
      continue;
    }

    // TODO(crbug.com/1091964): Implement Run on OS Login mode selection and
    // launch app appropriately
    apps::AppLaunchParams params(
        app_id, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromOsLogin);

    if (app_registrar_->GetAppEffectiveDisplayMode(app_id) ==
        blink::mojom::DisplayMode::kBrowser) {
      params.container = apps::LaunchContainer::kLaunchContainerTab;
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    }

    const std::string app_name = app_registrar_->GetAppShortName(app_id);

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
    Browser* browser,
    content::WebContents* web_contents,
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
  RunAppsOnOsLogin();
}

}  // namespace web_app
