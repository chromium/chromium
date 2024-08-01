// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_WEB_PUSH_WEB_PUSH_SENDER_H_
#define COMPONENTS_SHARING_MESSAGE_WEB_PUSH_WEB_PUSH_SENDER_H_

#include "components/sharing_message/web_push/web_push_common.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace crypto {
class ECPrivateKey;
}

struct WebPushMessage;

// Class for sending a message via Firebase Cloud Messaging (FCM) Web Push.
class WebPushSender {
 public:
  explicit WebPushSender(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  WebPushSender(const WebPushSender&) = delete;
  WebPushSender& operator=(const WebPushSender&) = delete;

  virtual ~WebPushSender();

  // Sends a WebPushMessage via FCM Web Push. Authenticates with FCM server
  // using Voluntary Application Server Identification for Web Push (VAPID)
  // protocol.
  // |fcm_token|: FCM registration token for receiving end.
  // |vapid_key|: Private key to sign VAPID header.
  // |message|: WebPushMessage to be sent.
  // |callback|: To be invoked with message_id if asynchronous operation
  // succeeded, or std::nullopt if operation failed.
  virtual void SendMessage(const std::string& fcm_token,
                           crypto::ECPrivateKey* vapid_key,
                           WebPushMessage message,
                           WebPushCallback callback);

 private:
  void OnMessageSent(std::unique_ptr<network::SimpleURLLoader> url_loader,
                     WebPushCallback callback,
                     std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<WebPushSender> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SHARING_MESSAGE_WEB_PUSH_WEB_PUSH_SENDER_H_
