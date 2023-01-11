// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/message_dispatcher_bridge.h"

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace messages {

namespace {

// Simply makes the MessageDispatcherBridge destructor public so that it can be
// instantiated by tests.
class MessageDispatcherBridgeForTesting : public MessageDispatcherBridge {
 public:
  ~MessageDispatcherBridgeForTesting() override = default;
};

}  // namespace

TEST(MessageDispatcherBridgeTest, IsMessagesEnabledForEmbedder) {
  MessageDispatcherBridgeForTesting message_dispatcher_bridge;

  EXPECT_FALSE(message_dispatcher_bridge.IsMessagesEnabledForEmbedder());

  message_dispatcher_bridge.Initialize(
      base::BindRepeating([](int) { return -1; }));

  EXPECT_TRUE(message_dispatcher_bridge.IsMessagesEnabledForEmbedder());
}

}  // namespace messages
