// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_REPLY_PROXY_H_
#define COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_REPLY_PROXY_H_

#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace content {
class Page;
}

namespace js_injection {

// Used to send messages to the page.
class WebMessageReplyProxy {
 public:
  // To match the JavaScript call, this function would ideally be named
  // PostMessage(), but that conflicts with a Windows macro, so PostWebMessage()
  // is used.
  virtual void PostWebMessage(blink::WebMessagePayload message) = 0;

  // Returns the page the messages are sent to.
  virtual content::Page& GetPage() = 0;

 protected:
  virtual ~WebMessageReplyProxy() = default;
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_REPLY_PROXY_H_
