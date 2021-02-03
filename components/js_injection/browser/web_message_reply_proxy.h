// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_REPLY_PROXY_H_
#define COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_REPLY_PROXY_H_

#include "base/strings/string16.h"

namespace js_injection {

struct WebMessage;

// Used to send messages to the page.
class WebMessageReplyProxy {
 public:
  virtual void PostMessage(std::unique_ptr<WebMessage> message) = 0;

  // Returns true if the page associated with the channel is in the back
  // forward cache.
  virtual bool IsInBackForwardCache() = 0;

 protected:
  virtual ~WebMessageReplyProxy() = default;
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_REPLY_PROXY_H_
