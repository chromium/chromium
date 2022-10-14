// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_H_
#define COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_H_

#include <string>

namespace js_injection {

// A struct representing mojo type `js_injection.mojom.JsWebMessage`.
struct JsWebMessage {
  JsWebMessage();
  JsWebMessage(JsWebMessage&) = delete;
  JsWebMessage(JsWebMessage&&);
  JsWebMessage& operator=(JsWebMessage&) = delete;
  JsWebMessage& operator=(JsWebMessage&&);

  std::u16string string;
};
}  // namespace js_injection

#endif
