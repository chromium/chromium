// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webui/cast_webui_message_handler.h"

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
    const std::string& function,
    std::vector<base::Value> args) {
  AllowJavascript();
  javascript_called_ = true;
  std::vector<const base::Value*> args_copy;
  for (const auto& arg : args) {
    args_copy.push_back(&arg);
  }
  WebUIMessageHandler::CallJavascriptFunction(function, args_copy);
}

}  // namespace chromecast
