// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_H_
#define COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/push_notification/push_notification_client_id.h"

namespace push_notification {

// Base class for PushNotificationService clients to. Each feature that uses the
// PushNotificationService must create its own PushNotificationClient and
// implement the OnMessageReceived functionality. PushNotificationClient must
// register with the PushNotificationService to begin receiving messages.
class PushNotificationClient {
 public:
  explicit PushNotificationClient(ClientId client_id) : client_id_(client_id) {}
  virtual ~PushNotificationClient() = default;

  // Returns the feature's `client_id_`.
  ClientId GetClientId() { return client_id_; }

  // Only called when the instance of the `message_data` matches the
  // `client_id_`.
  virtual void OnMessageReceived(
      base::flat_map<std::string, std::string> message_data) = 0;

 private:
  // The enum that is used to associate incoming push notifications to
  // their destination feature. This identifier is converted to a unique string
  // using `GetClientIdStr()`, this string must match the identifier used inside
  // the notification's payload when sending the notification to the push
  // notification server.
  const ClientId client_id_;
};

}  // namespace push_notification

#endif  // COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_CLIENT_H_
