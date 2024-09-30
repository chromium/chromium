// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_MANAGER_H_
#define COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/push_notification/push_notification_client.h"

namespace push_notification {

// `PushNotificationClientManager` is responsible for delegating notifications
// to the corresponding features who have a registered `PushNotificationClient`.
// Messages received before the corresponding `PushNotificationClient` are
// delivered to that client once registered via the client calling
// `CheckPendingMessageStore()`.
class PushNotificationClientManager {
 public:
  PushNotificationClientManager();
  ~PushNotificationClientManager();

  struct PushNotificationMessage {
    PushNotificationMessage();
    PushNotificationMessage(const PushNotificationMessage& other);
    PushNotificationMessage(PushNotificationMessage&& other);
    ~PushNotificationMessage();

    PushNotificationMessage& operator=(PushNotificationMessage&& other);

    // Key Value map to contain all relevant information intended for the
    // `PushNotificationClient`.
    base::flat_map<std::string, std::string> data;
    std::string collapse_key;
    std::string sender_id;
    std::string message_id;
    std::string raw_data;

    // Whether the contents of the message have been decrypted, and are
    // available in |raw_data|.
    bool decrypted = false;
  };

  void AddPushNotificationClient(PushNotificationClient* client);
  void RemovePushNotificationClient(ClientId client_id);
  std::vector<const PushNotificationClient*> GetPushNotificationClients();
  void NotifyPushNotificationClientOfMessage(PushNotificationMessage message);

 private:
  void FlushPendingMessageStore(ClientId client_id);

  base::flat_map<ClientId, raw_ptr<PushNotificationClient, CtnExperimental>>
      client_id_to_client_map_;

  // Messages for clients that have not registered with the service yet. After a
  // client registers, `FlushPendingMessageStore()` iterates through this vector
  // and checks if there are any pending messages which should immediately be
  // directed to the new client.
  std::vector<PushNotificationMessage> pending_message_store_;
};

}  // namespace push_notification

#endif  // COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_MANAGER_H_
