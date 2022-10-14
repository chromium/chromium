// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/common/web_message.h"

#include <string>

namespace js_injection {

JsWebMessage::JsWebMessage() = default;

JsWebMessage::JsWebMessage(JsWebMessage&&) = default;

JsWebMessage& JsWebMessage::operator=(JsWebMessage&&) = default;

}  // namespace js_injection
