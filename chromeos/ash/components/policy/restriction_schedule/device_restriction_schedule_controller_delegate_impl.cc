// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller_delegate_impl.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/user_manager/user_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr const char kDeviceRestrictionScheduleNotifierId[] =
    "device_restriction_schedule";

}  // namespace

DeviceRestrictionScheduleControllerDelegateImpl::
    DeviceRestrictionScheduleControllerDelegateImpl() = default;

bool DeviceRestrictionScheduleControllerDelegateImpl::IsUserLoggedIn() const {
  using user_manager::UserManager;
  return (UserManager::IsInitialized() && UserManager::Get()->IsUserLoggedIn());
}

void DeviceRestrictionScheduleControllerDelegateImpl::
    ShowUpcomingLogoutNotification(base::Time logout_time) {
  const std::u16string time_to_display = base::TimeFormatTimeOfDay(logout_time);
  const std::u16string title = l10n_util::GetStringFUTF16(
      IDS_DEVICE_RESTRICTION_SCHEDULE_UPCOMING_LOGOUT_NOTIFICATION_TITLE,
      time_to_display);
  const std::u16string body = l10n_util::GetStringUTF16(
      IDS_DEVICE_RESTRICTION_SCHEDULE_UPCOMING_LOGOUT_NOTIFICATION_BODY);

  message_center::RichNotificationData data;
  data.never_timeout = true;
  data.fullscreen_visibility = message_center::FullscreenVisibility::OVER_USER;
  data.priority = message_center::NotificationPriority::SYSTEM_PRIORITY;

  auto notifier_id = message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kDeviceRestrictionScheduleNotifierId,
      ash::NotificationCatalogName::kDeviceRestrictionScheduleUpcomingLogout);

  message_center::MessageCenter::Get()->AddNotification(
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kUpcomingLogoutNotificationId, title, body,
          std::u16string() /* display_source */, GURL() /* origin_url */,
          notifier_id, data, nullptr /* delegate */,
          vector_icons::kBusinessIcon,
          message_center::SystemNotificationWarningLevel::WARNING));
}

void DeviceRestrictionScheduleControllerDelegateImpl::
    ShowPostLogoutNotification() {
  const std::u16string domain =
      base::UTF8ToUTF16(ash::InstallAttributes::Get()->GetDomain());

  const std::u16string title = l10n_util::GetStringUTF16(
      IDS_DEVICE_RESTRICTION_SCHEDULE_POST_LOGOUT_NOTIFICATION_TITLE);
  const std::u16string body = l10n_util::GetStringFUTF16(
      IDS_DEVICE_RESTRICTION_SCHEDULE_POST_LOGOUT_NOTIFICATION_BODY, domain,
      ui::GetChromeOSDeviceName());

  message_center::RichNotificationData data;
  data.never_timeout = true;
  data.fullscreen_visibility = message_center::FullscreenVisibility::OVER_USER;
  data.priority = message_center::NotificationPriority::SYSTEM_PRIORITY;

  auto notifier_id = message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kDeviceRestrictionScheduleNotifierId,
      ash::NotificationCatalogName::kDeviceRestrictionSchedulePostLogout);

  message_center::MessageCenter::Get()->AddNotification(
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kPostLogoutNotificationId,
          title, body, std::u16string() /* display_source */,
          GURL() /* origin_url */, notifier_id, data, nullptr /* delegate */,
          vector_icons::kBusinessIcon,
          message_center::SystemNotificationWarningLevel::WARNING));
}

}  // namespace policy
