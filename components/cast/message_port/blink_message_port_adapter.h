// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_MESSAGE_PORT_BLINK_MESSAGE_PORT_ADAPTER_H_
#define COMPONENTS_CAST_MESSAGE_PORT_BLINK_MESSAGE_PORT_ADAPTER_H_

#include <memory>
#include <vector>

#include "components/cast/message_port/message_port.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"

namespace cast_api_bindings {

// Adapts between blink::WebMessagePort and the port type created by
// cast_api_bindings::CreatePlatformMessagePortPair. Some Javascript engines
// specifically use blink::WebMessagePort internally but need
// cast_api_bindings::MessagePort(s) to communicate with Cast bindings.
class BlinkMessagePortAdapter {
 public:
  BlinkMessagePortAdapter() = delete;

  // Adapts a |client| blink port to the port type created by
  // CreatePlatformMessagePortPair, which is returned.
  static std::unique_ptr<MessagePort> ToClientPlatformMessagePort(
      blink::WebMessagePort&& client);

  // Adapts a |server| port of the type created by CreatePlatformMessagePortPair
  // to a blink port, which is returned.
  static blink::WebMessagePort FromServerPlatformMessagePort(
      std::unique_ptr<MessagePort> server);
};

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_MESSAGE_PORT_BLINK_MESSAGE_PORT_ADAPTER_H_
