// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/updates/update_notification_client.h"

namespace updates {

UpdateNotificationClient::UpdateNotificationClient() = default;

UpdateNotificationClient::~UpdateNotificationClient() = default;

void UpdateNotificationClient::BeforeShowNotification(
    std::unique_ptr<NotificationData> notification_data,
    NotificationDataCallback callback) {
  NOTIMPLEMENTED();
}

void UpdateNotificationClient::OnSchedulerInitialized(
    bool success,
    std::set<std::string> guid) {
  NOTIMPLEMENTED();
}

void UpdateNotificationClient::OnUserAction(const UserActionData& action_data) {
  NOTIMPLEMENTED();
}

}  // namespace updates
