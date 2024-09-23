// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/push_notification/push_notification_client_manager.h"

#include "base/logging.h"

namespace {

const char kClientIdKey[] = "type_id";

}  // namespace

namespace push_notification {

PushNotificationClientManager::PushNotificationClientManager() {}

PushNotificationClientManager::~PushNotificationClientManager() = default;

void PushNotificationClientManager::AddPushNotificationClient(
    PushNotificationClient* client) {
  CHECK(client);
  CHECK(GetClientIdStr(client->GetClientId()));
  client_id_to_client_map_.insert_or_assign(client->GetClientId(), client);
  FlushPendingMessageStore(client->GetClientId());
}

void PushNotificationClientManager::RemovePushNotificationClient(
    ClientId client_id) {
  client_id_to_client_map_.erase(client_id);
}

std::vector<const PushNotificationClient*>
PushNotificationClientManager::GetPushNotificationClients() {
  std::vector<const PushNotificationClient*> client_list;
  for (const auto& pair : client_id_to_client_map_) {
    client_list.push_back(pair.second);
  }
  return client_list;
}

void PushNotificationClientManager::NotifyPushNotificationClientOfMessage(
    PushNotificationMessage message) {
  auto client_id = GetClientIdFromStr(message.data.at(kClientIdKey));

  if (!client_id.has_value()) {
    VLOG(1)
        << __func__
        << "Received a message for an invalid client ID. Message discarded.";
    return;
  }

  if (client_id_to_client_map_.contains(client_id)) {
    client_id_to_client_map_.at(client_id)->OnMessageReceived(message.data);
  } else {
    pending_message_store_.push_back(std::move(message));
  }
}

void PushNotificationClientManager::FlushPendingMessageStore(
    ClientId client_id) {
  // Iterate through the `pending_message_store`, dispatch any messages that
  // belong to the client being registered and remove dispatched messages.
  auto iter = pending_message_store_.begin();
  std::optional<std::string> client_id_str = GetClientIdStr(client_id);
  if (!client_id_str.has_value()) {
    return;
  }
  while (iter != pending_message_store_.end()) {
    if (iter->data.at(kClientIdKey) == client_id_str) {
      client_id_to_client_map_.at(client_id)->OnMessageReceived(iter->data);
      iter = pending_message_store_.erase(iter);
    } else {
      ++iter;
    }
  }
}

PushNotificationClientManager::PushNotificationMessage::
    PushNotificationMessage() = default;
PushNotificationClientManager::PushNotificationMessage::PushNotificationMessage(
    PushNotificationMessage&& other) = default;
PushNotificationClientManager::PushNotificationMessage::PushNotificationMessage(
    const PushNotificationMessage& other) = default;
PushNotificationClientManager::PushNotificationMessage&
PushNotificationClientManager::PushNotificationMessage::operator=(
    PushNotificationMessage&& other) = default;
PushNotificationClientManager::PushNotificationMessage::
    ~PushNotificationMessage() = default;

}  // namespace push_notification
