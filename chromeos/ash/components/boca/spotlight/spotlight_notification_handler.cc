// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_notification_handler.h"

#include <memory>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_notification_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

using message_center::MessageCenter;
using message_center::Notification;
using message_center::NotificationType;
using message_center::NotifierId;
using message_center::NotifierType;
using message_center::SystemNotificationWarningLevel;

namespace ash::boca {

void SpotlightNotificationHandler::Delegate::ShowNotification(
    std::unique_ptr<message_center::Notification> notification) {
  MessageCenter::Get()->AddNotification(std::move(notification));
}

void SpotlightNotificationHandler::Delegate::ClearNotification(
    const std::string& notification_id) {
  MessageCenter::Get()->RemoveNotification(notification_id, /*by_user=*/false);
}

SpotlightNotificationHandler::SpotlightNotificationHandler(
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      timer_(base::RepeatingTimer(
          FROM_HERE,
          kSpotlightNotificationCountdownInterval,
          base::BindRepeating(&SpotlightNotificationHandler::
                                  StartSpotlightCountdownNotificationInternal,
                              base::Unretained(this)))) {}

SpotlightNotificationHandler::~SpotlightNotificationHandler() = default;

void SpotlightNotificationHandler::StartSpotlightCountdownNotification(
    CountdownCompletionCallback completion_callback) {
  if (timer_.IsRunning()) {
    StopSpotlightCountdown();
  }
  completion_callback_ = std::move(completion_callback);
  notification_duration_ = kSpotlightNotificationDuration;
  timer_.Reset();
}

void SpotlightNotificationHandler::StopSpotlightCountdown() {
  if (completion_callback_) {
    completion_callback_.Reset();
  }
  timer_.Stop();
  delegate_->ClearNotification(kSpotlightStartedNotificationId);
}

void SpotlightNotificationHandler::
    StartSpotlightCountdownNotificationInternal() {
  if (!completion_callback_ || !timer_.IsRunning()) {
    // If there is no callback, the final timer has already ran.
    return;
  }

  if (!notification_duration_.is_positive()) {
    timer_.Stop();
    // Clear pre-existing notifications with the same id if still present.
    delegate_->ClearNotification(kSpotlightStartedNotificationId);
    std::move(completion_callback_).Run();
    return;
  }
  // TODO: dorianbrandon - Update logo to CT logo when ready.
  delegate_->ShowNotification(CreateSystemNotificationPtr(
      NotificationType::NOTIFICATION_TYPE_SIMPLE,
      kSpotlightStartedNotificationId,
      l10n_util::GetStringUTF16(IDS_BOCA_NOTIFICATION_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_BOCA_SPOTLIGHT_NOTIFICATION_MESSAGE,
          base::NumberToString16(notification_duration_.InSeconds())),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      NotifierId(NotifierType::SYSTEM_COMPONENT, kSpotlightNotifierId,
                 ash::NotificationCatalogName::kBocaSpotlightStarted),
      message_center::RichNotificationData(), /*delegate=*/nullptr,
      ash::kNotificationTimerIcon, SystemNotificationWarningLevel::NORMAL));

  notification_duration_ =
      notification_duration_ - kSpotlightNotificationCountdownInterval;
}
}  // namespace ash::boca
