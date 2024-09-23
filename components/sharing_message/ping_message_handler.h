// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_PING_MESSAGE_HANDLER_H_
#define COMPONENTS_SHARING_MESSAGE_PING_MESSAGE_HANDLER_H_

#include "components/sharing_message/sharing_message_handler.h"

class PingMessageHandler : public SharingMessageHandler {
 public:
  PingMessageHandler();

  PingMessageHandler(const PingMessageHandler&) = delete;
  PingMessageHandler& operator=(const PingMessageHandler&) = delete;

  ~PingMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(components_sharing_message::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;
};

#endif  // COMPONENTS_SHARING_MESSAGE_PING_MESSAGE_HANDLER_H_
