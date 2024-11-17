// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"

#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/public/cpp/notification.h"

using extensions::ExtensionRegistry;

namespace {

const char kQuitWithAppsOriginUrl[] = "chrome://quit-with-apps";
const int kQuitAllAppsButtonIndex = 0;
const int kDontShowAgainButtonIndex = 1;

void CloseNotification(Profile* profile) {
  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      NotificationHandler::Type::TRANSIENT,
      QuitWithAppsController::kQuitWithAppsNotificationID);
}

}  // namespace

const char QuitWithAppsController::kQuitWithAppsNotificationID[] =
    "quit-with-apps";

QuitWithAppsController::QuitWithAppsController() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  message_center::ButtonInfo quit_apps_button_info(
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_QUIT_LABEL));
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(quit_apps_button_info);
  message_center::ButtonInfo suppression_button_info(
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_SUPPRESSION_LABEL));
  rich_notification_data.buttons.push_back(suppression_button_info);

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, kQuitWithAppsNotificationID,
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_TITLE),
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_EXPLANATION),
      ui::ImageModel::FromImage(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              IDR_PRODUCT_LOGO_128)),
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_NOTIFICATION_DISPLAY_SOURCE),
      GURL(kQuitWithAppsOriginUrl),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kQuitWithAppsNotificationID),
      rich_notification_data, this);
  if (ProfileManager* profile_manager = g_browser_process->profile_manager()) {
    profile_manager_observation_.Observe(profile_manager);
  }
}

QuitWithAppsController::~QuitWithAppsController() {}

void QuitWithAppsController::OnProfileManagerDestroying() {
  // Set `notification_profile_` to null to avoid danling pointer detection when
  // ProfileManager is destroyed.
  notification_profile_ = nullptr;
  profile_manager_observation_.Reset();
}

void QuitWithAppsController::Close(bool by_user) {
  if (by_user)
    suppress_for_session_ = true;
}

void QuitWithAppsController::Click(const std::optional<int>& button_index,
                                   const std::optional<std::u16string>& reply) {
  CloseNotification(notification_profile_);

  if (!button_index)
    return;

  if (*button_index == kQuitAllAppsButtonIndex) {
    AppWindowRegistryUtil::CloseAllAppWindows();
  } else if (*button_index == kDontShowAgainButtonIndex) {
    g_browser_process->local_state()->SetBoolean(
        prefs::kNotifyWhenAppsKeepChromeAlive, false);
  }
}

bool QuitWithAppsController::ShouldQuit() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Quit immediately if this is a test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAppsKeepChromeAliveInTests)) {
    return true;
  }

  // Quit immediately if Chrome is restarting.
  if (g_browser_process->local_state()->GetBoolean(
          prefs::kRestartLastSessionOnShutdown)) {
    return true;
  }

  // Quit immediately if there are no windows.
  if (!AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(
          extensions::AppWindow::WINDOW_TYPE_DEFAULT)) {
    return true;
  }

  // If there are browser windows, and this notification has been suppressed for
  // this session or permanently, then just return false to prevent Chrome from
  // quitting. If there are no browser windows, always show the notification.
  bool suppress_always = !g_browser_process->local_state()->GetBoolean(
      prefs::kNotifyWhenAppsKeepChromeAlive);
  if (!BrowserList::GetInstance()->empty() &&
      (suppress_for_session_ || suppress_always)) {
    return false;
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  DCHECK(profiles.size());

  // Delete any existing notification to ensure this one is shown. If
  // notification_profile_ is NULL then it must be that no notification has been
  // added by this class yet.
  if (notification_profile_)
    CloseNotification(notification_profile_);
  notification_profile_ = profiles[0];
  NotificationDisplayServiceFactory::GetForProfile(notification_profile_)
      ->Display(NotificationHandler::Type::TRANSIENT, *notification_,
                /*metadata=*/nullptr);

  // Always return false, the notification UI can be used to quit all apps which
  // will cause Chrome to quit.
  return false;
}

// static
void QuitWithAppsController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kNotifyWhenAppsKeepChromeAlive, true);
}
