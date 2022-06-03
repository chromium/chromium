// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/mock_message_dispatcher_bridge.h"

namespace messages {

MockMessageDispatcherBridge::MockMessageDispatcherBridge() = default;
MockMessageDispatcherBridge::~MockMessageDispatcherBridge() = default;

int MockMessageDispatcherBridge::MapToJavaDrawableId(int resource_id) {
  return -1;
}

}  // namespace messages
