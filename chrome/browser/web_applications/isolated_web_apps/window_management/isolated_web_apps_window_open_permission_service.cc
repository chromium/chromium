// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_window_open_permission_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
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
#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_window_open_permission_service_delegate.h"
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

constexpr base::TimeDelta kNotificationDismissalTime = base::Seconds(20);

constexpr std::string_view
    kIsolatedWebAppsWindowOpenPermissionNotificationPattern =
        "isolated_web_apps_window_open_permission_notification_%s";

std::string GetAppName(Profile* profile, const webapps::AppId& app_id) {
  return web_app::WebAppProvider::GetForWebApps(profile)
      ->registrar_unsafe()
      .GetAppShortName(app_id);
}

std::string GetNotificationIdForApp(const webapps::AppId& app_id) {
  return base::StringPrintf(
      kIsolatedWebAppsWindowOpenPermissionNotificationPattern, app_id);
}

}  // namespace

bool ShouldShowNotificationForWindowOpen(
    const std::optional<IsolationData::OpenedTabsCounterNotificationState>&
        state) {
  if (state) {
    return !state->acknowledged() &&
           (state->times_shown() < kMaxNotificationShowCount);
  }

  return true;
}

IsolatedWebAppsWindowOpenPermissionService::
    IsolatedWebAppsWindowOpenPermissionService(Profile* profile)
    : profile_(*profile),
      provider_(web_app::WebAppProvider::GetForWebApps(profile)) {
  provider()->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&IsolatedWebAppsWindowOpenPermissionService::
                                    RetrieveNotificationStates,
                                weak_ptr_factory_.GetWeakPtr()));
}

void IsolatedWebAppsWindowOpenPermissionService::RetrieveNotificationStates() {
  provider()->scheduler().ScheduleCallback(
      "RetrieveIwaNotificationStates", web_app::AllAppsLockDescription(),
      base::BindOnce(&IsolatedWebAppsWindowOpenPermissionService::
                         OnAllAppsLockAcquiredForStateRetrieval,
                     weak_ptr_factory_.GetWeakPtr()),
      /*on_complete=*/base::DoNothing());
}

void IsolatedWebAppsWindowOpenPermissionService::
    OnAllAppsLockAcquiredForStateRetrieval(web_app::AllAppsLock& lock,
                                           base::DictValue& debug_value) {
  for (const WebApp& web_app : lock.registrar().GetApps(
           WebAppFilter::IsIsolatedWebAppWithOnlyUserManagement())) {
    const auto& state =
        web_app.isolation_data()->opened_tabs_counter_notification_state();
    if (state && ShouldShowNotificationForWindowOpen(state)) {
      notification_states_cache_.emplace(web_app.app_id(), *state);
    }
  }
}

IsolatedWebAppsWindowOpenPermissionService::
    ~IsolatedWebAppsWindowOpenPermissionService() = default;

void IsolatedWebAppsWindowOpenPermissionService::Shutdown() {
  std::vector<webapps::AppId> app_ids_to_close(
      apps_with_active_notifications_.begin(),
      apps_with_active_notifications_.end());

  for (const webapps::AppId& app_id : app_ids_to_close) {
    CloseNotification(app_id);
  }

  // CloseNotification() should have removed all `app_ids` from
  // `apps_with_active_notifications_` under the hood.
  CHECK(apps_with_active_notifications_.empty());
}

void IsolatedWebAppsWindowOpenPermissionService::OnWebContentsCreated(
    const webapps::AppId& opener_app_id) {
  const web_app::WebApp* iwa = provider()->registrar_unsafe().GetAppById(
      opener_app_id, WebAppFilter::IsIsolatedWebAppWithOnlyUserManagement());

  if (!iwa ||
      !ShouldShowNotificationForWindowOpen(
          iwa->isolation_data()->opened_tabs_counter_notification_state())) {
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
  } else {
    notification_state = IsolationData::OpenedTabsCounterNotificationState(
        notification_state.acknowledged(),
        notification_state.times_shown() + 1);

    apps_with_active_notifications_.insert(opener_app_id);

    PersistNotificationState(opener_app_id, notification_state);
    CreateAndDisplayNotification(opener_app_id);
  }

  auto& timer = dismissal_timers_[opener_app_id];
  timer.Start(
      FROM_HERE, kNotificationDismissalTime,
      base::BindOnce(
          &IsolatedWebAppsWindowOpenPermissionService::CloseNotification,
          weak_ptr_factory_.GetWeakPtr(), opener_app_id));
}

void IsolatedWebAppsWindowOpenPermissionService::OnNotificationAcknowledged(
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

void IsolatedWebAppsWindowOpenPermissionService::CreateAndDisplayNotification(
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
      base::MakeRefCounted<IsolatedWebAppsWindowOpenPermissionServiceDelegate>(
          profile(), app_id,
          base::BindRepeating(&IsolatedWebAppsWindowOpenPermissionService::
                                  OnNotificationAcknowledged,
                              weak_ptr_factory_.GetWeakPtr()),
          base::BindRepeating(
              &IsolatedWebAppsWindowOpenPermissionService::CloseNotification,
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

void IsolatedWebAppsWindowOpenPermissionService::PersistNotificationState(
    const webapps::AppId& app_id,
    const web_app::IsolationData::OpenedTabsCounterNotificationState&
        current_notification_state) {
  provider()->scheduler().ScheduleCallback(
      "IsolatedWebAppsWindowOpenPermissionService::PersistNotificationState",
      web_app::AppLockDescription(app_id),
      base::BindOnce(
          [](const webapps::AppId& app_id,
             const web_app::IsolationData::OpenedTabsCounterNotificationState&
                 notification_state,
             web_app::AppLock& lock, base::DictValue& debug_value) {
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

void IsolatedWebAppsWindowOpenPermissionService::CloseNotification(
    const webapps::AppId& app_id) {
  dismissal_timers_.erase(app_id);
  apps_with_active_notifications_.erase(app_id);

  NotificationDisplayServiceFactory::GetForProfile(profile())->Close(
      NotificationHandler::Type::TRANSIENT, GetNotificationIdForApp(app_id));
}

}  // namespace web_app
