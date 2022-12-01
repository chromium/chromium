// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_notification_interaction_handler.h"

#include "chromeos/ash/components/phonehub/notification.h"

namespace ash {
namespace phonehub {

FakeNotificationInteractionHandler::FakeNotificationInteractionHandler() =
    default;

FakeNotificationInteractionHandler::~FakeNotificationInteractionHandler() =
    default;

void FakeNotificationInteractionHandler::HandleNotificationClicked(
    int64_t notification_id,
    const Notification::AppMetadata& app_metadata) {
  handled_notification_count_++;
}

void FakeNotificationInteractionHandler::AddNotificationClickHandler(
    NotificationClickHandler* handler) {
  notification_click_handler_count_++;
}

void FakeNotificationInteractionHandler::RemoveNotificationClickHandler(
    NotificationClickHandler* handler) {
  notification_click_handler_count_--;
}

}  // namespace phonehub
}  // namespace ash
