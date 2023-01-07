// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/mock_message_dispatcher_bridge.h"

namespace messages {

MockMessageDispatcherBridge::MockMessageDispatcherBridge() = default;
MockMessageDispatcherBridge::~MockMessageDispatcherBridge() = default;

int MockMessageDispatcherBridge::MapToJavaDrawableId(int resource_id) {
  return -1;
}

void MockMessageDispatcherBridge::SetMessagesEnabledForEmbedder(
    bool messages_enabled_for_embedder) {
  messages_enabled_for_embedder_ = messages_enabled_for_embedder;
}

}  // namespace messages
