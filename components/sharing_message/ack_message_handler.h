// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_ACK_MESSAGE_HANDLER_H_
#define COMPONENTS_SHARING_MESSAGE_ACK_MESSAGE_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/sharing_message/sharing_message_handler.h"

class SharingMessageSender;

// Class to managae ack message and notify observers.
class AckMessageHandler : public SharingMessageHandler {
 public:
  explicit AckMessageHandler(SharingMessageSender* sharing_message_sender);

  AckMessageHandler(const AckMessageHandler&) = delete;
  AckMessageHandler& operator=(const AckMessageHandler&) = delete;

  ~AckMessageHandler() override;

  // SharingMessageHandler implementation:
  void OnMessage(components_sharing_message::SharingMessage message,
                 SharingMessageHandler::DoneCallback done_callback) override;

 private:
  raw_ptr<SharingMessageSender> sharing_message_sender_;
};

#endif  // COMPONENTS_SHARING_MESSAGE_ACK_MESSAGE_HANDLER_H_
