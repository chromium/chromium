// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/push_notification/fake_push_notification_client.h"

#include "components/push_notification/push_notification_constants.h"

namespace push_notification {

FakePushNotificationClient::FakePushNotificationClient(
    const push_notification::ClientId& client_id)
    : push_notification::PushNotificationClient(client_id) {}

FakePushNotificationClient::~FakePushNotificationClient() = default;

void FakePushNotificationClient::OnMessageReceived(
    base::flat_map<std::string, std::string> message_data) {
  last_received_message_data_ = message_data;
}

const base::flat_map<std::string, std::string>&
FakePushNotificationClient::GetMostRecentMessageDataReceived() {
  return last_received_message_data_;
}

}  // namespace push_notification
