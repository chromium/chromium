// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service.h"

#include <algorithm>
#include <memory>
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

// The time window to check for a burst of opened tabs.
constexpr base::TimeDelta kBurstWindow = base::Seconds(5);
// The number of tabs opened within the window to trigger a notification.
constexpr int kBurstThreshold = 3;

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

struct IsolatedWebAppsOpenedTabsCounterService::TrackedTabData {
  TrackedTabData(const webapps::AppId& app_id,
                 base::Time creation_time,
                 std::unique_ptr<TabObserver> observer)
      : app_id(app_id),
        creation_time(creation_time),
        observer(std::move(observer)) {}
  ~TrackedTabData() = default;

  TrackedTabData(TrackedTabData&&) = default;
  TrackedTabData& operator=(TrackedTabData&&) = default;

  webapps::AppId app_id;
  base::Time creation_time;
  std::unique_ptr<TabObserver> observer;
};

void IsolatedWebAppsOpenedTabsCounterService::TabObserver::
    WebContentsDestroyed() {
  service_->HandleTabClosure(web_contents());
}

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
    : profile_(*profile) {
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);

  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(
          &IsolatedWebAppsOpenedTabsCounterService::RetrieveNotificationStates,
          weak_ptr_factory_.GetWeakPtr()));
}

void IsolatedWebAppsOpenedTabsCounterService::RetrieveNotificationStates() {
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile());

  provider->scheduler().ScheduleCallback(
      "RetrieveIwaNotificationStates", web_app::AllAppsLockDescription(),
      base::BindOnce(&IsolatedWebAppsOpenedTabsCounterService::
                         OnAllAppsLockAcquiredForStateRetrieval,
                     weak_ptr_factory_.GetWeakPtr()),
      /*on_complete=*/base::DoNothing());
}

void IsolatedWebAppsOpenedTabsCounterService::
    OnAllAppsLockAcquiredForStateRetrieval(web_app::AllAppsLock& lock,
                                           base::Value::Dict& debug_value) {
  for (const webapps::AppId& app_id : lock.registrar().GetAppIds()) {
    const web_app::WebApp* web_app = lock.registrar().GetAppById(app_id);
    if (web_app && ShouldShowNotificationForWindowOpen(*web_app)) {
      if (const auto& state = web_app->isolation_data()
                                  ->opened_tabs_counter_notification_state()) {
        notification_states_cache_.emplace(app_id, *state);
      }
    }
  }
}

IsolatedWebAppsOpenedTabsCounterService::
    ~IsolatedWebAppsOpenedTabsCounterService() = default;

void IsolatedWebAppsOpenedTabsCounterService::Shutdown() {
  for (const auto& [app_id, _] : app_tab_timestamps_) {
    CloseNotification(app_id);
  }

  app_tab_timestamps_.clear();
  tracked_tabs_.clear();
}

void IsolatedWebAppsOpenedTabsCounterService::OnWebContentsCreated(
    const webapps::AppId& opener_app_id,
    content::WebContents* new_contents,
    base::Time navigation_start_time) {
  if (!base::Contains(notification_states_cache_, opener_app_id)) {
    const web_app::WebApp* web_app =
        web_app::WebAppProvider::GetForWebApps(profile())
            ->registrar_unsafe()
            .GetAppById(opener_app_id);

    CHECK(web_app);
    if (!ShouldShowNotificationForWindowOpen(*web_app)) {
      return;
    }
  }

  if (tracked_tabs_
          .try_emplace(new_contents, opener_app_id, navigation_start_time,
                       std::make_unique<TabObserver>(new_contents, this))
          .second) {
    AddTabTimestampForApp(opener_app_id, navigation_start_time);
    UpdateOrRemoveNotificationForOpener(opener_app_id);
  }
}

void IsolatedWebAppsOpenedTabsCounterService::HandleTabClosure(
    content::WebContents* contents) {
  auto it = tracked_tabs_.find(contents);
  if (it == tracked_tabs_.end()) {
    return;
  }

  webapps::AppId opener_app_id = it->second.app_id;
  base::Time creation_time = it->second.creation_time;
  tracked_tabs_.erase(it);

  RemoveTabTimestampForApp(opener_app_id, creation_time);
  UpdateOrRemoveNotificationForOpener(opener_app_id);
}

void IsolatedWebAppsOpenedTabsCounterService::AddTabTimestampForApp(
    const webapps::AppId& app_id,
    base::Time timestamp) {
  app_tab_timestamps_[app_id].push_back(timestamp);
}

void IsolatedWebAppsOpenedTabsCounterService::RemoveTabTimestampForApp(
    const webapps::AppId& app_id,
    base::Time timestamp) {
  auto it = app_tab_timestamps_.find(app_id);
  if (it == app_tab_timestamps_.end()) {
    return;
  }

  auto& timestamps = it->second;
  std::erase(timestamps, timestamp);

  if (timestamps.empty()) {
    app_tab_timestamps_.erase(it);
  }
}

void IsolatedWebAppsOpenedTabsCounterService::
    UpdateOrRemoveNotificationForOpener(const webapps::AppId& app_id) {
  int tab_count = 0;

  if (auto it = app_tab_timestamps_.find(app_id);
      it != app_tab_timestamps_.end()) {
    auto& timestamps = it->second;

    // Prune timestamps older than the burst window.
    const base::Time cutoff =
        web_app::WebAppProvider::GetForWebApps(profile())->clock().Now() -
        kBurstWindow;
    std::erase_if(timestamps,
                  [cutoff](const base::Time& t) { return t < cutoff; });
    tab_count = timestamps.size();
    if (tab_count == 0) {
      app_tab_timestamps_.erase(it);
    }
  }

  // If there are no longer any tracked tabs for this app, close the
  // corresponding notification if it's active.
  if (tab_count == 0) {
    if (apps_with_active_notifications_.contains(app_id)) {
      CloseNotification(app_id);
    }
    return;
  }

  auto notification_state_it = notification_states_cache_.find(app_id);

  if (notification_state_it == notification_states_cache_.end()) {
    notification_state_it =
        notification_states_cache_
            .emplace(app_id, IsolationData::OpenedTabsCounterNotificationState(
                                 /*acknowledged=*/false,
                                 /*times_shown=*/0))
            .first;
  }
  auto& notification_state = notification_state_it->second;

  if (notification_state.acknowledged() ||
      notification_state.times_shown() >= kMaxNotificationShowCount) {
    return;
  }

  if (tab_count >= kBurstThreshold) {
    if (!apps_with_active_notifications_.contains(app_id)) {
      RegisterFirstTimeActiveAppNotification(app_id);
    }
    CreateAndDisplayNotification(app_id, tab_count);
  }
}

void IsolatedWebAppsOpenedTabsCounterService::
    RegisterFirstTimeActiveAppNotification(const webapps::AppId& app_id) {
  auto it = notification_states_cache_.find(app_id);

  if (it == notification_states_cache_.end()) {
    return;
  }

  auto& notification_state = it->second;

  notification_state = IsolationData::OpenedTabsCounterNotificationState(
      notification_state.acknowledged(), notification_state.times_shown() + 1);

  apps_with_active_notifications_.insert(app_id);

  PersistNotificationState(app_id);
}

void IsolatedWebAppsOpenedTabsCounterService::OnNotificationAcknowledged(
    const webapps::AppId& app_id) {
  auto it = notification_states_cache_.find(app_id);
  int times_shown =
      (it != notification_states_cache_.end()) ? it->second.times_shown() : 0;

  notification_states_cache_.insert_or_assign(
      app_id, IsolationData::OpenedTabsCounterNotificationState(
                  /*acknowledged=*/true, times_shown));

  CloseNotification(app_id);
  PersistNotificationState(app_id);
}

void IsolatedWebAppsOpenedTabsCounterService::CreateAndDisplayNotification(
    const webapps::AppId& app_id,
    int current_tab_count) {
  std::string app_name = GetAppName(profile(), app_id);
  std::u16string title = l10n_util::GetStringFUTF16(
      IDS_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_NOTIFICATION_TITLE,
      base::UTF8ToUTF16(app_name));

  std::u16string message = l10n_util::GetStringFUTF16Int(
      IDS_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_NOTIFICATION_MESSAGE,
      current_tab_count);

  message_center::RichNotificationData rich_data;
  message_center::ButtonInfo content_settings_button_info;
  content_settings_button_info.title = l10n_util::GetStringUTF16(
      IDS_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_NOTIFICATION_BUTTON_SETTINGS);
  rich_data.buttons.push_back(content_settings_button_info);

  message_center::ButtonInfo close_button_info;
  close_button_info.title = l10n_util::GetStringUTF16(
      IDS_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_NOTIFICATION_CLOSE_BUTTON);
  rich_data.buttons.push_back(close_button_info);

  scoped_refptr<message_center::NotificationDelegate> delegate =
      base::MakeRefCounted<IsolatedWebAppsOpenedTabsCounterServiceDelegate>(
          profile(), app_id,
          base::BindRepeating(&IsolatedWebAppsOpenedTabsCounterService::
                                  CloseAllWebContentsOpenedByApp,
                              weak_ptr_factory_.GetWeakPtr()),
          base::BindRepeating(&IsolatedWebAppsOpenedTabsCounterService::
                                  OnNotificationAcknowledged,
                              weak_ptr_factory_.GetWeakPtr()),
          base::BindRepeating(
              &IsolatedWebAppsOpenedTabsCounterService::CloseNotification,
              weak_ptr_factory_.GetWeakPtr(), app_id));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, GetNotificationIdForApp(app_id),
      title, message,
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

void IsolatedWebAppsOpenedTabsCounterService::CloseAllWebContentsOpenedByApp(
    const webapps::AppId& app_id) {
  std::vector<content::WebContents*> web_contents_to_close;
  for (auto const& [web_contents, tab_data] : tracked_tabs_) {
    if (tab_data.app_id == app_id) {
      web_contents_to_close.push_back(web_contents);
    }
  }

  for (content::WebContents* web_contents : web_contents_to_close) {
    // Closing the WebContents will trigger its observer, which in turn will
    // call HandleTabClosure, removing the contents from `tracked_tabs_` and
    // removing the timestamp.
    web_contents->Close();
  }
}

void IsolatedWebAppsOpenedTabsCounterService::PersistNotificationState(
    const webapps::AppId& app_id) {
  auto it = notification_states_cache_.find(app_id);
  if (it == notification_states_cache_.end()) {
    return;
  }
  const web_app::IsolationData::OpenedTabsCounterNotificationState
      current_notification_state = it->second;

  web_app::WebAppProvider::GetForWebApps(profile())
      ->scheduler()
      .ScheduleCallback(
          "IsolatedWebAppsOpenedTabsCounterService::PersistNotificationState",
          web_app::AppLockDescription(app_id),
          base::BindOnce(
              [](const webapps::AppId& app_id,
                 const web_app::IsolationData::
                     OpenedTabsCounterNotificationState&
                         current_notification_state,
                 web_app::AppLock& lock, base::Value::Dict& debug_value) {
                web_app::ScopedRegistryUpdate update =
                    lock.sync_bridge().BeginUpdate();

                web_app::WebApp* web_app = update->UpdateApp(app_id);
                CHECK(web_app && web_app->isolation_data().has_value());
                web_app->SetIsolationData(
                    web_app::IsolationData::Builder(*web_app->isolation_data())
                        .SetOpenedTabsCounterNotificationState(
                            current_notification_state)
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
