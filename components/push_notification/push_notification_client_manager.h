// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_MANAGER_H_
#define COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_MANAGER_H_

#include "base/containers/flat_map.h"
#include "components/push_notification/push_notification_client.h"

namespace push_notification {

// PushNotificationClientManager is responsible for delegating notifications to
// the corresponding features who have a registered PushNotificationClient.
class PushNotificationClientManager {
 public:
  PushNotificationClientManager();
  virtual ~PushNotificationClientManager();

  struct PushNotificationMessage {
    PushNotificationMessage();
    PushNotificationMessage(const PushNotificationMessage& other) = delete;
    PushNotificationMessage(PushNotificationMessage&& other);
    virtual ~PushNotificationMessage();

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

  virtual void NotifyPushNotificationClientOfMessage(
      PushNotificationMessage message) = 0;

 protected:
  base::flat_map<ClientId, PushNotificationClient*> client_id_to_client_map_;
};

}  // namespace push_notification

#endif  // COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_MANAGER_H_
