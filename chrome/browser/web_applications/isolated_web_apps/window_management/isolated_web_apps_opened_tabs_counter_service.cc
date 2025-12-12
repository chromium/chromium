// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service_delegate.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/grit/generated_resources.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace web_app {

namespace {

constexpr int kMaxNotificationShowCount = 3;

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

bool ShouldShowNotificationForWindowOpen(const web_app::WebApp& web_app) {
  if (!web_app.isolation_data()) {
    return false;
  }

  const bool is_managed =
      web_app.GetSources().HasAny({web_app::WebAppManagement::kKiosk,
                                   web_app::WebAppManagement::kIwaShimlessRma,
                                   web_app::WebAppManagement::kIwaPolicy});
  if (is_managed) {
    return false;
  }
  if (const auto& state =
          web_app.isolation_data()->opened_tabs_counter_notification_state()) {
    return !state->acknowledged() &&
           (state->times_shown() < kMaxNotificationShowCount);
  }

  return true;
}

IsolatedWebAppsOpenedTabsCounterService::
    IsolatedWebAppsOpenedTabsCounterService(Profile* profile)
    : profile_(*profile),
      provider_(web_app::WebAppProvider::GetForWebApps(profile)) {
  provider()->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(
          &IsolatedWebAppsOpenedTabsCounterService::RetrieveNotificationStates,
          weak_ptr_factory_.GetWeakPtr()));
}

void IsolatedWebAppsOpenedTabsCounterService::RetrieveNotificationStates() {
  provider()->scheduler().ScheduleCallback(
      "RetrieveIwaNotificationStates", web_app::AllAppsLockDescription(),
      base::BindOnce(&IsolatedWebAppsOpenedTabsCounterService::
                         OnAllAppsLockAcquiredForStateRetrieval,
                     weak_ptr_factory_.GetWeakPtr()),
      /*on_complete=*/base::DoNothing());
}

void IsolatedWebAppsOpenedTabsCounterService::
    OnAllAppsLockAcquiredForStateRetrieval(web_app::AllAppsLock& lock,
                                           base::Value::Dict& debug_value) {
  for (const WebApp& web_app :
       lock.registrar().GetApps(WebAppFilter::IsIsolatedApp())) {
    const auto& state =
        web_app.isolation_data()->opened_tabs_counter_notification_state();
    if (state && ShouldShowNotificationForWindowOpen(web_app)) {
      notification_states_cache_.emplace(web_app.app_id(), *state);
    }
  }
}

IsolatedWebAppsOpenedTabsCounterService::
    ~IsolatedWebAppsOpenedTabsCounterService() = default;

void IsolatedWebAppsOpenedTabsCounterService::Shutdown() {
  for (const webapps::AppId& app_id : apps_with_active_notifications_) {
    CloseNotification(app_id);
  }

  apps_with_active_notifications_.clear();
}

void IsolatedWebAppsOpenedTabsCounterService::OnWebContentsCreated(
    const webapps::AppId& opener_app_id) {
  const web_app::WebApp* web_app =
      provider()->registrar_unsafe().GetAppById(opener_app_id);

  if (!web_app || !ShouldShowNotificationForWindowOpen(*web_app)) {
    return;
  }

  // Check if the notification is currently being displayed (active).
  bool is_notification_already_active =
      apps_with_active_notifications_.contains(opener_app_id);

  auto [it, inserted] =
      notification_states_cache_.try_emplace(opener_app_id,
                                             /*acknowledged=*/false,
                                             /*times_shown=*/0);
  auto& notification_state = it->second;

  // Final check to see if we should proceed at all (acknowledged or max shown).
  if (notification_state.acknowledged() ||
      notification_state.times_shown() >= kMaxNotificationShowCount) {
    return;
  }

  if (is_notification_already_active) {
    CloseNotification(opener_app_id);
    // CloseNotification removes it from apps_with_active_notifications_.
    // It must be re-added.
    apps_with_active_notifications_.insert(opener_app_id);
    CreateAndDisplayNotification(opener_app_id);

    return;
  }

  notification_state = IsolationData::OpenedTabsCounterNotificationState(
      notification_state.acknowledged(), notification_state.times_shown() + 1);

  apps_with_active_notifications_.insert(opener_app_id);

  PersistNotificationState(opener_app_id, notification_state);
  CreateAndDisplayNotification(opener_app_id);
}

void IsolatedWebAppsOpenedTabsCounterService::OnNotificationAcknowledged(
    const webapps::AppId& app_id) {
  auto it = notification_states_cache_.find(app_id);
  int times_shown =
      (it != notification_states_cache_.end()) ? it->second.times_shown() : 0;

  auto state = IsolationData::OpenedTabsCounterNotificationState(
      /*acknowledged=*/true, times_shown);
  notification_states_cache_.insert_or_assign(app_id, state);
  PersistNotificationState(app_id, state);
  CloseNotification(app_id);
}

void IsolatedWebAppsOpenedTabsCounterService::CreateAndDisplayNotification(
    const webapps::AppId& app_id) {
  std::string app_name = GetAppName(profile(), app_id);
  std::u16string title = l10n_util::GetStringFUTF16(
      IDS_ISOLATED_WEB_APPS_WINDOW_OPEN_PERMISSION_NOTIFICATION_TITLE,
      base::UTF8ToUTF16(app_name));

  message_center::RichNotificationData rich_data;
  message_center::ButtonInfo content_settings_button_info;
  content_settings_button_info.title = l10n_util::GetStringUTF16(
      IDS_ISOLATED_WEB_APPS_WINDOW_OPEN_PERMISSION_NOTIFICATION_BUTTON_SETTINGS);
  rich_data.buttons.push_back(content_settings_button_info);

  scoped_refptr<message_center::NotificationDelegate> delegate =
      base::MakeRefCounted<IsolatedWebAppsOpenedTabsCounterServiceDelegate>(
          profile(), app_id,
          base::BindRepeating(&IsolatedWebAppsOpenedTabsCounterService::
                                  OnNotificationAcknowledged,
                              weak_ptr_factory_.GetWeakPtr()),
          base::BindRepeating(
              &IsolatedWebAppsOpenedTabsCounterService::CloseNotification,
              weak_ptr_factory_.GetWeakPtr()));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, GetNotificationIdForApp(app_id),
      title,
      l10n_util::GetStringUTF16(
          IDS_ISOLATED_WEB_APPS_WINDOW_OPEN_PERMISSION_NOTIFICATION_MESSAGE),
      /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      /*notifier_id=*/
      message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                 app_id),
      /*optional_fields=*/rich_data, delegate);

  NotificationDisplayServiceFactory::GetForProfile(profile())->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      /*metadata=*/nullptr);
}

void IsolatedWebAppsOpenedTabsCounterService::PersistNotificationState(
    const webapps::AppId& app_id,
    const web_app::IsolationData::OpenedTabsCounterNotificationState&
        current_notification_state) {
  provider()->scheduler().ScheduleCallback(
      "IsolatedWebAppsOpenedTabsCounterService::PersistNotificationState",
      web_app::AppLockDescription(app_id),
      base::BindOnce(
          [](const webapps::AppId& app_id,
             const web_app::IsolationData::OpenedTabsCounterNotificationState&
                 notification_state,
             web_app::AppLock& lock, base::Value::Dict& debug_value) {
            web_app::ScopedRegistryUpdate update =
                lock.sync_bridge().BeginUpdate();

            web_app::WebApp* web_app = update->UpdateApp(app_id);
            if (!web_app || !web_app->isolation_data().has_value()) {
              return;
            }

            web_app->SetIsolationData(
                web_app::IsolationData::Builder(*web_app->isolation_data())
                    .SetOpenedTabsCounterNotificationState(notification_state)
                    .Build());
          },
          app_id, current_notification_state),
      base::DoNothing());
}

void IsolatedWebAppsOpenedTabsCounterService::CloseNotification(
    const webapps::AppId& app_id) {
  apps_with_active_notifications_.erase(app_id);

  NotificationDisplayServiceFactory::GetForProfile(profile())->Close(
      NotificationHandler::Type::TRANSIENT, GetNotificationIdForApp(app_id));
}

}  // namespace web_app
