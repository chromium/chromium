// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_notification_service.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace web_app {

namespace {

constexpr char kIwaUpdateNotificationIdPrefix[] = "iwa_pending_update_";
constexpr char kIwaUpdateNotifierId[] = "isolated_web_app_update";

std::string GetNotificationIdForApp(const webapps::AppId& app_id) {
  return kIwaUpdateNotificationIdPrefix + app_id;
}

}  // namespace

IsolatedWebAppUpdateNotificationService::
    IsolatedWebAppUpdateNotificationService(Profile& profile,
                                            WebAppProvider& provider)
    : profile_(profile), provider_(provider) {
  install_manager_observation_.Observe(&provider_->install_manager());
  update_manager_observation_.Observe(
      &provider_->isolated_web_app_update_manager());
}

IsolatedWebAppUpdateNotificationService::
    ~IsolatedWebAppUpdateNotificationService() = default;

void IsolatedWebAppUpdateNotificationService::ShowUpdatePendingNotification(
    const webapps::AppId& app_id) {
  if (!base::FeatureList::IsEnabled(
          ash::features::kIsolatedWebAppInlineUpdate)) {
    return;
  }
  if (chromeos::IsKioskSession()) {
    return;
  }

  std::unique_ptr<message_center::Notification> notification =
      CreateUpdatePendingNotification(app_id);
  if (!notification) {
    return;
  }

  NotificationDisplayServiceFactory::GetForProfile(&*profile_)
      ->Display(NotificationHandler::Type::TRANSIENT, *notification,
                /*metadata=*/nullptr);
}

std::unique_ptr<message_center::Notification>
IsolatedWebAppUpdateNotificationService::CreateUpdatePendingNotification(
    const webapps::AppId& app_id) {
  const WebApp* app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!app || !app->isolation_data().has_value() ||
      !app->isolation_data()->pending_update_info().has_value()) {
    return nullptr;
  }

  std::u16string app_name =
      base::UTF8ToUTF16(provider_->registrar_unsafe().GetAppShortName(app_id));
  std::u16string version_str = base::UTF8ToUTF16(
      app->isolation_data()->pending_update_info()->version.GetString());

  std::u16string title = l10n_util::GetStringFUTF16(
      IDS_IWA_UPDATE_PENDING_NOTIFICATION_TITLE, app_name);

  std::u16string message = l10n_util::GetStringFUTF16(
      IDS_IWA_UPDATE_PENDING_NOTIFICATION_MESSAGE, version_str);

  message_center::RichNotificationData rich_data;
  message_center::ButtonInfo restart_button;
  restart_button.title = l10n_util::GetStringUTF16(
      IDS_IWA_UPDATE_PENDING_NOTIFICATION_RESTART_BUTTON);
  rich_data.buttons.push_back(restart_button);

#if BUILDFLAG(IS_CHROMEOS)
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kIwaUpdateNotifierId,
      ash::NotificationCatalogName::kIsolatedWebAppUpdate);
#else
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kIwaUpdateNotifierId);
#endif

  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &IsolatedWebAppUpdateNotificationService::OnNotificationClick,
              weak_factory_.GetWeakPtr(), app_id));

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, GetNotificationIdForApp(app_id),
      title, message,
      /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(), notifier_id, rich_data, delegate);

  notification->SetSystemPriority();
  return notification;
}

void IsolatedWebAppUpdateNotificationService::CloseNotification(
    const webapps::AppId& app_id) {
  NotificationDisplayServiceFactory::GetForProfile(&*profile_)
      ->Close(NotificationHandler::Type::TRANSIENT,
              GetNotificationIdForApp(app_id));
}

void IsolatedWebAppUpdateNotificationService::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  CloseNotification(app_id);
  restarted_apps_.erase(app_id);
}

void IsolatedWebAppUpdateNotificationService::OnUpdateApplyTaskCompleted(
    const webapps::AppId& app_id,
    IsolatedWebAppApplyUpdateCommandResult status) {
  if (restarted_apps_.contains(app_id)) {
    restarted_apps_.erase(app_id);
    if (status.has_value()) {
      provider_->scheduler().LaunchApp(app_id, /*url=*/std::nullopt,
                                       /*callback=*/base::DoNothing(),
                                       apps::LaunchSource::kFromChromeInternal);
    }
  }
}

void IsolatedWebAppUpdateNotificationService::OnNotificationClick(
    const webapps::AppId& app_id,
    std::optional<int> button_index) {
  if (button_index.has_value() && *button_index == 0) {
    // "Restart app" button clicked.
    restarted_apps_.insert(app_id);
    provider_->ui_manager().CloseAppWindows(app_id);
    CloseNotification(app_id);
  }
}

}  // namespace web_app
