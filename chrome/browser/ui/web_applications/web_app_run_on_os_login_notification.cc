// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_run_on_os_login_notification.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/check_deref.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/user_manager/user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace {

constexpr size_t kAppLength = 18;

std::unique_ptr<message_center::Notification> CreateNotification(
    const base::flat_map<webapps::AppId,
                         web_app::WebAppUiManager::RoolNotificationBehavior>&
        apps,
    base::WeakPtr<Profile> profile) {
  CHECK(apps.size() > 0);

  const auto& [first_app_id, first_app_behavior] = *apps.begin();
  CHECK(first_app_behavior.is_rool_enabled);

  web_app::WebAppRegistrar& registrar =
      web_app::WebAppProvider::GetForWebApps(profile.get())->registrar_unsafe();
  const std::u16string truncated_app_name = gfx::TruncateString(
      base::UTF8ToUTF16(registrar.GetAppShortName(first_app_id)), kAppLength,
      gfx::BreakType::WORD_BREAK);

  std::u16string title = base::i18n::MessageFormatter::FormatWithNamedArgs(
      l10n_util::GetStringUTF16(IDS_RUN_ON_OS_LOGIN_ENABLED_TITLE),
      /*name0=*/"NUM_ROOL_APPS", static_cast<int>(apps.size()),
      /*name1=*/"APP_NAME_1", truncated_app_name);

  std::u16string message;
  // Cases:
  // - One app, is_prevent_close_enabled = true: Point out which app is
  // autostarted and unclosable.
  // - One app, is_prevent_close_enabled = false: Point out which app is
  // autostarted.
  // - Multiple apps, one or more has is_prevent_close_enabled = true: Generic
  // message that multiple apps were autostarted and are unclosable.
  // - Multiple apps, no app has is_prevent_close_enabled = true: Generic
  // message that multiple apps were autostarted.
  if (apps.size() == 1) {
    if (first_app_behavior.is_prevent_close_enabled) {
      message = base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_RUN_ON_OS_LOGIN_ENABLED_ONE_APP_ROOL_AND_PREVENTCLOSE_MESSAGE),
          "APP_NAME", truncated_app_name);
    } else {
      message = base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_RUN_ON_OS_LOGIN_ENABLED_NO_PREVENTCLOSE_MESSAGE),
          "NUM_ROOL_APPS", 1, "APP_NAME", truncated_app_name);
    }
  } else {
    if (std::ranges::any_of(apps, [](const auto& app) {
          return app.second.is_prevent_close_enabled;
        })) {
      message = base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_RUN_ON_OS_LOGIN_ENABLED_MULTIPLE_APPS_ROOL_AND_PREVENTCLOSE_MESSAGE));
    } else {
      message = base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_RUN_ON_OS_LOGIN_ENABLED_NO_PREVENTCLOSE_MESSAGE),
          "NUM_ROOL_APPS", static_cast<int>(apps.size()), "APP_NAME",
          truncated_app_name);
    }
  }

  auto notification_data = message_center::RichNotificationData();
  notification_data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_RUN_ON_OS_LOGIN_ENABLED_LEARN_MORE));

  const user_manager::User& user = CHECK_DEREF(
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile.get()));

  message_center::NotifierId notifier_id =
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 web_app::kRunOnOsLoginNotifierId,
                                 ash::NotificationCatalogName::kWebAppSettings);
  // Set `profile_id` so that the notification is hidden while another
  // signed-in user is active.
  notifier_id.profile_id = user.GetAccountId().GetUserEmail();

  auto click_callback = base::BindRepeating(
      [](base::WeakPtr<Profile> profile, std::optional<int> index) -> void {
        if (!profile) {
          return;
        }

        NavigateParams params(
            profile.get(), GURL(chrome::kChromeUIManagementURL),
            ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
        params.disposition = WindowOpenDisposition::kNewForegroundTab;
        params.window_action = NavigateParams::WindowAction::kShowWindow;
        Navigate(&params);
      },
      profile);

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      ash::CreateUserScopedNotificationId(web_app::kRunOnOsLoginNotificationId,
                                          user.username_hash()),
      title, message, ui::ImageModel(),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(), notifier_id, notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          std::move(click_callback)));

  notification->SetSystemPriority();

  return notification;
}
}  // namespace

namespace web_app {

const char kRunOnOsLoginNotificationId[] = "run_on_os_login";
const char kRunOnOsLoginNotifierId[] = "run_on_os_login_notifier";

void DisplayRunOnOsLoginNotification(
    const base::flat_map<webapps::AppId,
                         WebAppUiManager::RoolNotificationBehavior>& apps,
    base::WeakPtr<Profile> profile) {
  if (!profile) {
    return;
  }

  message_center::MessageCenter::Get()->AddNotification(
      CreateNotification(apps, profile));
}
}  // namespace web_app
