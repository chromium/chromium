// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"

#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
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
  NotificationDisplayService::GetForProfile(profile)->Close(
      NotificationHandler::Type::TRANSIENT,
      QuitWithAppsController::kQuitWithAppsNotificationID);
}

}  // namespace

const char QuitWithAppsController::kQuitWithAppsNotificationID[] =
    "quit-with-apps";

QuitWithAppsController::QuitWithAppsController()
    : hosted_app_quit_notification_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kHostedAppQuitNotification)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  message_center::ButtonInfo quit_apps_button_info(
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_QUIT_LABEL));
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.push_back(quit_apps_button_info);
  if (!hosted_app_quit_notification_) {
    message_center::ButtonInfo suppression_button_info(
        l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_SUPPRESSION_LABEL));
    rich_notification_data.buttons.push_back(suppression_button_info);
  }

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, kQuitWithAppsNotificationID,
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_TITLE),
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_EXPLANATION),
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_PRODUCT_LOGO_128),
      l10n_util::GetStringUTF16(IDS_QUIT_WITH_APPS_NOTIFICATION_DISPLAY_SOURCE),
      GURL(kQuitWithAppsOriginUrl),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kQuitWithAppsNotificationID),
      rich_notification_data, this);
}

QuitWithAppsController::~QuitWithAppsController() {}

void QuitWithAppsController::Close(bool by_user) {
  if (by_user)
    suppress_for_session_ = !hosted_app_quit_notification_;
}

void QuitWithAppsController::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  CloseNotification(notification_profile_);

  if (!button_index)
    return;

  if (*button_index == kQuitAllAppsButtonIndex) {
    if (hosted_app_quit_notification_) {
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
          content::NotificationService::AllSources(),
          content::NotificationService::NoDetails());
      chrome::CloseAllBrowsers();
    }
    AppWindowRegistryUtil::CloseAllAppWindows();
  } else if (*button_index == kDontShowAgainButtonIndex &&
             !hosted_app_quit_notification_) {
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

  if (hosted_app_quit_notification_) {
    bool hosted_apps_open = false;
    for (Browser* browser : *BrowserList::GetInstance()) {
      if (!browser->deprecated_is_app())
        continue;

      ExtensionRegistry* registry = ExtensionRegistry::Get(browser->profile());
      const extensions::Extension* extension = registry->GetExtensionById(
          web_app::GetAppIdFromApplicationName(browser->app_name()),
          ExtensionRegistry::ENABLED);
      if (extension->is_hosted_app()) {
        hosted_apps_open = true;
        break;
      }
    }

    // Quit immediately if there are no packaged app windows or hosted apps open
    // or the confirmation has been suppressed. Ignore panels.
    if (!AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(
            extensions::AppWindow::WINDOW_TYPE_DEFAULT) &&
        !hosted_apps_open) {
      return true;
    }
  } else {
    // Quit immediately if there are no windows or the confirmation has been
    // suppressed.
    if (!AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(
            extensions::AppWindow::WINDOW_TYPE_DEFAULT))
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
  NotificationDisplayService::GetForProfile(notification_profile_)
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
