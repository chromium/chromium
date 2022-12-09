// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_H_
#define COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_H_

#include <vector>

#include "third_party/blink/public/common/messaging/message_port_descriptor.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace js_injection {

// Represents a message to or from the page.
struct WebMessage {
  WebMessage();
  ~WebMessage();

  blink::WebMessagePayload message;
  std::vector<blink::MessagePortDescriptor> ports;
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_H_
