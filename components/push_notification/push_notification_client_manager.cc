// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/push_notification/push_notification_client_manager.h"

namespace push_notification {

PushNotificationClientManager::PushNotificationClientManager() = default;
PushNotificationClientManager::~PushNotificationClientManager() = default;

void PushNotificationClientManager::AddPushNotificationClient(
    PushNotificationClient* client) {
  CHECK(client);
  // TODO(b/287340843): implement adding a PushNotification client to the
  // `clients` map.
}

void PushNotificationClientManager::RemovePushNotificationClient(
    ClientId client_id) {
  // TODO(b/287340843): implement removing a PushNotification client from the
  // `clients` map.
}

std::vector<const PushNotificationClient*>
PushNotificationClientManager::GetPushNotificationClients() {
  std::vector<const PushNotificationClient*> client_list;
  for (const auto& pair : client_id_to_client_map_) {
    client_list.push_back(pair.second);
  }
  return client_list;
}

PushNotificationClientManager::PushNotificationMessage::
    PushNotificationMessage() = default;

PushNotificationClientManager::PushNotificationMessage::PushNotificationMessage(
    PushNotificationMessage&& other) = default;

PushNotificationClientManager::PushNotificationMessage::
    ~PushNotificationMessage() = default;

}  // namespace push_notification
