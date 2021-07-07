// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MOCK_MESSAGE_DISPATCHER_BRIDGE_H_
#define COMPONENTS_MESSAGES_ANDROID_MOCK_MESSAGE_DISPATCHER_BRIDGE_H_

#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/message_enums.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace messages {
class MockMessageDispatcherBridge : public MessageDispatcherBridge {
 public:
  MockMessageDispatcherBridge();
  ~MockMessageDispatcherBridge() override;

  MOCK_METHOD(bool,
              EnqueueMessage,
              (MessageWrapper * message,
               content::WebContents* web_contents,
               MessageScopeType scope_type,
               MessagePriority priority),
              (override));
  MOCK_METHOD(void,
              DismissMessage,
              (MessageWrapper * message,
               content::WebContents* web_contents,
               DismissReason dismiss_reason),
              (override));
};

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MOCK_MESSAGE_DISPATCHER_BRIDGE_H_
