// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_relaunch_notification.h"

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#endif

namespace web_app {

namespace {

constexpr char kWebAppRelaunchId[] = "web_app_relaunch";
constexpr char kWebAppRelaunchNotifierIdPrefix[] = "web_app_relaunch_notifier";

std::string CreateNotificationId(const webapps::AppId& placeholder_app_id) {
  return base::StrCat(
      {kWebAppRelaunchNotifierIdPrefix, ":", placeholder_app_id});
}

message_center::Notification CreateNotification(
    const webapps::AppId& placeholder_app_id,
    const webapps::AppId& final_app_id,
    const std::u16string& final_app_name) {
#if (BUILDFLAG(IS_CHROMEOS_ASH))
  message_center::NotifierId notifier_id = message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT, kWebAppRelaunchId,
      ash::NotificationCatalogName::kWebAppSettings);
#else
  message_center::NotifierId notifier_id = message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT, kWebAppRelaunchId);
#endif  // (BUILDFLAG(IS_CHROMEOS_ASH))

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      CreateNotificationId(placeholder_app_id),
      l10n_util::GetStringFUTF16(IDS_WEB_APP_RELAUNCH_NOTIFICATION_TITLE,
                                 final_app_name),
      l10n_util::GetStringUTF16(IDS_WEB_APP_RELAUNCH_NOTIFICATION_MESSAGE),
      ui::ImageModel(),
      /* display_source= */ std::u16string(),
      /* origin_url */ GURL(), std::move(notifier_id),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          message_center::HandleNotificationClickDelegate::ButtonClickCallback(
              base::DoNothing())));
  notification.SetSystemPriority();
  return notification;
}

}  // namespace

void NotifyAppRelaunchState(const webapps::AppId& placeholder_app_id,
                            const webapps::AppId& final_app_id,
                            const std::u16string& final_app_name,
                            base::WeakPtr<Profile> profile,
                            AppRelaunchState relaunch_state) {
  if (!profile) {
    return;
  }

  switch (relaunch_state) {
    case web_app::AppRelaunchState::kAppClosingForRelaunch:
      NotificationDisplayServiceFactory::GetForProfile(profile.get())
          ->Display(NotificationHandler::Type::TRANSIENT,
                    std::move(CreateNotification(placeholder_app_id,
                                                 final_app_id, final_app_name)),
                    /*metadata=*/nullptr);
      break;
    case web_app::AppRelaunchState::kAppAboutToRelaunch:
      // TODO(b/311711416): Implement progress bar.
      break;
    case web_app::AppRelaunchState::kAppRelaunched:
      scoped_refptr<base::SingleThreadTaskRunner> task_runner;
      if (GetTaskRunnerForTesting()) {            // IN-TEST
        task_runner = GetTaskRunnerForTesting();  // IN-TEST
      } else {
        task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
      }
      task_runner->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              [](webapps::AppId app_id, base::WeakPtr<Profile> profile) {
                if (!profile) {
                  return;
                }

                // The `NotificationDisplayService::Close` function can be
                // called even if
                // the notification is not shown anymore.
                NotificationDisplayServiceFactory::GetForProfile(profile.get())
                    ->Close(NotificationHandler::Type::TRANSIENT,
                            CreateNotificationId(app_id));
              },
              placeholder_app_id, std::move(profile)),
          base::Seconds(kSecondsToShowNotificationPostAppRelaunch));
      break;
  }
}

scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunnerForTesting() {
  static base::NoDestructor<scoped_refptr<base::SingleThreadTaskRunner>>
      task_runner_for_testing;
  return *task_runner_for_testing;
}

}  // namespace web_app
