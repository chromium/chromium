// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_MOCK_SHARING_MESSAGE_HANDLER_H_
#define COMPONENTS_SHARING_MESSAGE_MOCK_SHARING_MESSAGE_HANDLER_H_

#include "components/sharing_message/proto/sharing_message.pb.h"
#include "components/sharing_message/sharing_message_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockSharingMessageHandler : public SharingMessageHandler {
 public:
  MockSharingMessageHandler();
  MockSharingMessageHandler(const MockSharingMessageHandler&) = delete;
  MockSharingMessageHandler& operator=(const MockSharingMessageHandler&) =
      delete;
  ~MockSharingMessageHandler() override;

  // SharingMessageHandler:
  MOCK_METHOD2(OnMessage,
               void(components_sharing_message::SharingMessage, DoneCallback));
};

#endif  // COMPONENTS_SHARING_MESSAGE_MOCK_SHARING_MESSAGE_HANDLER_H_
