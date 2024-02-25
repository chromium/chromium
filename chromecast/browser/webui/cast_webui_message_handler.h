// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBUI_CAST_WEBUI_MESSAGE_HANDLER_H_
#define CHROMECAST_BROWSER_WEBUI_CAST_WEBUI_MESSAGE_HANDLER_H_

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromecast {

// Simple generic message handler for Web UIs. This class exposes a public
// method for running JS in the Web UI. This class is owned by the Web UI,
// but a reference is maintained inside of CastWebUI.
class CastWebUIMessageHandler : public content::WebUIMessageHandler {
 public:
  CastWebUIMessageHandler();
  ~CastWebUIMessageHandler() override;

  // content::WebUIMessageHandler implementation:
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

  // Invoke a JS function in the Web UI.
  void CallJavascriptFunction(std::string_view function,
                              base::span<const base::ValueView> args);

 private:
  bool javascript_called_ = false;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBUI_CAST_WEBUI_MESSAGE_HANDLER_H_
