// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webui/cast_webui_message_handler.h"

#include <string_view>

#include "base/logging.h"

namespace chromecast {

CastWebUIMessageHandler::CastWebUIMessageHandler() = default;

CastWebUIMessageHandler::~CastWebUIMessageHandler() = default;

void CastWebUIMessageHandler::RegisterMessages() {}

void CastWebUIMessageHandler::OnJavascriptDisallowed() {
  if (javascript_called_) {
    LOG(ERROR) << "The Web UI page navigated after JS was invoked externally. "
               << "This may be a bug.";
  }
}

void CastWebUIMessageHandler::CallJavascriptFunction(
    std::string_view function,
    base::span<const base::ValueView> args) {
  AllowJavascript();
  javascript_called_ = true;
  WebUIMessageHandler::CallJavascriptFunction(function, args);
}

}  // namespace chromecast
