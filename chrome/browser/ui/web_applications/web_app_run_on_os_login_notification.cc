// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_run_on_os_login_notification.h"

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#endif

namespace {

message_center::Notification CreateNotification(
    const std::vector<std::string>& app_names,
    base::WeakPtr<Profile> profile) {
  DCHECK(app_names.size() > 0);
  std::u16string title = base::i18n::MessageFormatter::FormatWithNamedArgs(
      l10n_util::GetStringUTF16(IDS_RUN_ON_OS_LOGIN_ENABLED_TITLE),
      /*name0=*/"NUM_ROOL_APPS", static_cast<int>(app_names.size()),
      /*name1=*/"APP_NAME_1", base::UTF8ToUTF16(app_names[0]));

  std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
      l10n_util::GetStringUTF16(IDS_RUN_ON_OS_LOGIN_ENABLED_MESSAGE),
      /*name0=*/"NUM_ROOL_APPS", static_cast<int>(app_names.size()),
      /*name1=*/"APP_NAME_1", base::UTF8ToUTF16(app_names[0]),
      /*name2=*/"APP_NAME_2",
      app_names.size() > 1 ? base::UTF8ToUTF16(app_names[1]) : u"",
      /*name3=*/"APP_NAME_3",
      app_names.size() > 2 ? base::UTF8ToUTF16(app_names[2]) : u"");

  auto notification_data = message_center::RichNotificationData();
  notification_data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_RUN_ON_OS_LOGIN_ENABLED_LEARN_MORE));

#if (BUILDFLAG(IS_CHROMEOS_ASH))
  message_center::NotifierId notifier_id =
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 web_app::kRunOnOsLoginNotifierId,
                                 ash::NotificationCatalogName::kWebAppSettings);
#else
  message_center::NotifierId notifier_id =
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 web_app::kRunOnOsLoginNotifierId);
#endif  // (BUILDFLAG(IS_CHROMEOS_ASH))

  base::RepeatingClosure click_callback = base::BindRepeating(
      [](base::WeakPtr<Profile> profile) {
        if (!profile) {
          return;
        }
        NavigateParams params(
            profile.get(), GURL(chrome::kChromeUIManagementURL),
            ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
        params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
        params.window_action = NavigateParams::SHOW_WINDOW;
        Navigate(&params);
      },
      profile);

  message_center::Notification notification{
      message_center::NOTIFICATION_TYPE_SIMPLE,
      std::string(web_app::kRunOnOsLoginNotificationId),
      title,
      message,
      ui::ImageModel(),
      /* display_source= */ std::u16string(),
      /* origin_url */ GURL(),
      notifier_id,
      notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          click_callback)};

  notification.SetSystemPriority();

  return notification;
}
}  // namespace

namespace web_app {

const char kRunOnOsLoginNotificationId[] = "run_on_os_login";
const char kRunOnOsLoginNotifierId[] = "run_on_os_login_notifier";

void DisplayRunOnOsLoginNotification(const std::vector<std::string>& app_names,
                                     base::WeakPtr<Profile> profile) {
  if (!profile) {
    return;
  }

  message_center::Notification notification =
      CreateNotification(app_names, profile);

  NotificationDisplayService::GetForProfile(profile.get())
      ->Display(NotificationHandler::Type ::TRANSIENT, notification,
                /*metadata=*/nullptr);
}
}  // namespace web_app
