// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_notification_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

namespace arc::arc_dlc_install_notification_manager {

namespace {

std::u16string GetMessage(NotificationType type) {
  switch (type) {
    case NotificationType::kArcVmPreloadStarted:
      return l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_STARTED_MESSAGE);
    case NotificationType::kArcVmPreloadSucceeded:
      return l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_SUCCEEDED_MESSAGE);
    case NotificationType::kArcVmPreloadFailed:
      return l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_FAILED_MESSAGE);
  }
}

std::string_view GetNotificationId(NotificationType type) {
  switch (type) {
    case NotificationType::kArcVmPreloadStarted:
      return kArcVmPreloadStartedId;
    case NotificationType::kArcVmPreloadSucceeded:
      return kArcVmPreloadSucceededId;
    case NotificationType::kArcVmPreloadFailed:
      return kArcVmPreloadFailedId;
  }
}
}  // namespace

void Show(NotificationType notification_type) {
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, "arc_dlc_install",
      ash::NotificationCatalogName::kArcDlcInstall);

  auto click_delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([]() {}));

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          std::string(GetNotificationId(notification_type)),
          l10n_util::GetStringUTF16(IDS_ARC_VM_PRELOAD_NOTIFICATION_TITLE),
          GetMessage(notification_type), std::u16string(), GURL(), notifier_id,
          message_center::RichNotificationData(), std::move(click_delegate),
          vector_icons::kSettingsIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

}  // namespace arc::arc_dlc_install_notification_manager
