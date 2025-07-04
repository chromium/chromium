// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service.h"

#include "base/containers/contains.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service_delegate.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

constexpr std::string_view
    kIsolatedWebAppsOpenedTabsCounterNotificationPattern =
        "isolated_web_apps_opened_tabs_counter_notification_%s";

std::string GetAppName(Profile* profile, const webapps::AppId& app_id) {
  return web_app::WebAppProvider::GetForWebApps(profile)
      ->registrar_unsafe()
      .GetAppShortName(app_id);
}

std::string GetNotificationIdForApp(const webapps::AppId& app_id) {
  return base::StringPrintf(
      kIsolatedWebAppsOpenedTabsCounterNotificationPattern, app_id);
}

}  // namespace

IsolatedWebAppsOpenedTabsCounterService::
    IsolatedWebAppsOpenedTabsCounterService(Profile* profile)
    : profile_(*profile) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile) {
      browser->tab_strip_model()->AddObserver(this);
    }
  }
  browser_list_observation_.Observe(BrowserList::GetInstance());
}

IsolatedWebAppsOpenedTabsCounterService::
    ~IsolatedWebAppsOpenedTabsCounterService() = default;

void IsolatedWebAppsOpenedTabsCounterService::Shutdown() {
  for (const auto& [app_id, _] : app_window_counts_) {
    NotificationDisplayServiceFactory::GetForProfile(profile())->Close(
        NotificationHandler::Type::TRANSIENT, GetNotificationIdForApp(app_id));
  }

  app_window_counts_.clear();
  opened_by_app_map_.clear();
}

void IsolatedWebAppsOpenedTabsCounterService::OnBrowserAdded(Browser* browser) {
  if (browser->profile() == profile()) {
    browser->tab_strip_model()->AddObserver(this);
  }
}

void IsolatedWebAppsOpenedTabsCounterService::OnBrowserRemoved(
    Browser* browser) {
  if (browser->profile() == profile()) {
    browser->tab_strip_model()->RemoveObserver(this);
  }
}

void IsolatedWebAppsOpenedTabsCounterService::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::Type::kInserted:
      for (const auto& content_with_id : change.GetInsert()->contents) {
        HandleOpenerCountIfTracked(content_with_id.contents);
      }
      break;
    case TabStripModelChange::Type::kRemoved:
      for (const auto& content_with_id : change.GetRemove()->contents) {
        // We only want to decrease the count if the tab was deleted, but not
        // when moved to another tab group.
        if (content_with_id.remove_reason ==
            TabStripModelChange::RemoveReason::kDeleted) {
          HandleTabOrWindowClosure(content_with_id.contents);
        }
      }
      break;
    default:
      break;
  }
}

void IsolatedWebAppsOpenedTabsCounterService::HandleOpenerCountIfTracked(
    content::WebContents* contents) {
  ASSIGN_OR_RETURN(webapps::AppId opener_app_id,
                   MaybeGetOpenerIsolatedWebAppId(contents), [] {});

  if (base::Contains(opened_by_app_map_, contents)) {
    return;
  }

  IncrementWindowCountForApp(opener_app_id);
  opened_by_app_map_[contents] = opener_app_id;
  UpdateOrRemoveNotificationForOpener(opener_app_id);
}

void IsolatedWebAppsOpenedTabsCounterService::HandleTabOrWindowClosure(
    content::WebContents* contents) {
  // If WebContents were not opened by an IWA then there is nothing to do.
  if (!base::Contains(opened_by_app_map_, contents)) {
    return;
  }
  // Stop tracking closed WebContents and update the count of opened child
  // WebContents for its opener.
  webapps::AppId opener_app_id = opened_by_app_map_[contents];
  opened_by_app_map_.erase(contents);
  DecrementWindowCountForApp(opener_app_id);
  UpdateOrRemoveNotificationForOpener(opener_app_id);
}

std::optional<webapps::AppId>
IsolatedWebAppsOpenedTabsCounterService::MaybeGetOpenerIsolatedWebAppId(
    content::WebContents* contents) {
  content::RenderFrameHost* opener_rfh = contents->GetOpener();
  if (!opener_rfh) {
    return std::nullopt;
  }

  content::WebContents* opener_web_contents =
      content::WebContents::FromRenderFrameHost(opener_rfh);

  if (!opener_web_contents) {
    return std::nullopt;
  }

  // Check if the opener is an IWA that is not policy-installed.
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile());

  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(opener_web_contents);
  if (app_id && provider->registrar_unsafe().IsIsolated(*app_id) &&
      !provider->registrar_unsafe().AppMatches(
          *app_id, web_app::WebAppFilter::PolicyInstalledIsolatedWebApp())) {
    return *app_id;
  }

  return std::nullopt;
}

void IsolatedWebAppsOpenedTabsCounterService::IncrementWindowCountForApp(
    const webapps::AppId& app_id) {
  app_window_counts_[app_id]++;
}

void IsolatedWebAppsOpenedTabsCounterService::DecrementWindowCountForApp(
    const webapps::AppId& app_id) {
  if (!base::Contains(app_window_counts_, app_id)) {
    return;
  }
  app_window_counts_[app_id]--;
  if (app_window_counts_[app_id] == 0) {
    app_window_counts_.erase(app_id);
  }
}

void IsolatedWebAppsOpenedTabsCounterService::
    UpdateOrRemoveNotificationForOpener(const webapps::AppId& app_id) {
  NotificationDisplayService* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile());

  auto it = app_window_counts_.find(app_id);

  // Close the notification if the app isn't found or has only one window.
  if (it == app_window_counts_.end() || it->second <= 1) {
    display_service->Close(NotificationHandler::Type::TRANSIENT,
                           GetNotificationIdForApp(app_id));
  } else {
    // Otherwise, update/create a notification for the app.
    CreateAndDisplayNotification(app_id, it->second);
  }
}
void IsolatedWebAppsOpenedTabsCounterService::CreateAndDisplayNotification(
    const webapps::AppId& app_id,
    int current_window_count) {
  std::string app_name = GetAppName(profile(), app_id);
  std::u16string title = l10n_util::GetStringFUTF16(
      IDS_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_NOTIFICATION_TITLE,
      base::UTF8ToUTF16(app_name));

  std::u16string message = l10n_util::GetStringFUTF16Int(
      IDS_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_NOTIFICATION_MESSAGE,
      current_window_count);

  message_center::RichNotificationData rich_data;
  message_center::ButtonInfo content_settings_button_info;
  content_settings_button_info.title = l10n_util::GetStringUTF16(
      IDS_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_NOTIFICATION_BUTTON_SETTINGS);
  rich_data.buttons.push_back(content_settings_button_info);

  scoped_refptr<message_center::NotificationDelegate> delegate =
      base::MakeRefCounted<IsolatedWebAppsOpenedTabsCounterServiceDelegate>(
          profile(), app_id);

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, GetNotificationIdForApp(app_id),
      title, message, /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      /*notifier_id=*/
      message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                 app_id),
      /*optional_fields=*/rich_data, delegate);

  NotificationDisplayServiceFactory::GetForProfile(profile())->Display(
      NotificationHandler::Type::TRANSIENT, notification, /*metadata=*/nullptr);
}
