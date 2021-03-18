// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/fake_notification_interaction_handler.h"

namespace chromeos {
namespace phonehub {

FakeNotificationInteractionHandler::FakeNotificationInteractionHandler() =
    default;

FakeNotificationInteractionHandler::~FakeNotificationInteractionHandler() =
    default;

void FakeNotificationInteractionHandler::HandleNotificationClicked(
    int64_t notification_id) {
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
}  // namespace chromeos
