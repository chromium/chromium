// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PUSH_NOTIFICATION_FAKE_PUSH_NOTIFICATION_CLIENT_H_
#define COMPONENTS_PUSH_NOTIFICATION_FAKE_PUSH_NOTIFICATION_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "components/push_notification/push_notification_client.h"

namespace push_notification {

class FakePushNotificationClient : public PushNotificationClient {
 public:
  explicit FakePushNotificationClient(
      const push_notification::ClientId& client_id);
  FakePushNotificationClient(const FakePushNotificationClient&) = delete;
  FakePushNotificationClient& operator=(const FakePushNotificationClient&) =
      delete;
  ~FakePushNotificationClient() override;

  void OnMessageReceived(
      base::flat_map<std::string, std::string> message_data) override;

  const base::flat_map<std::string, std::string>&
  GetMostRecentMessageDataReceived();

 private:
  base::flat_map<std::string, std::string> last_received_message_data_;
};

}  // namespace push_notification

#endif  // COMPONENTS_PUSH_NOTIFICATION_FAKE_PUSH_NOTIFICATION_CLIENT_H_
