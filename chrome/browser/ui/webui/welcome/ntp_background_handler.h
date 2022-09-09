// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_NTP_BACKGROUND_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_NTP_BACKGROUND_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace welcome {

class NtpBackgroundHandler : public content::WebUIMessageHandler {
 public:
  NtpBackgroundHandler();

  NtpBackgroundHandler(const NtpBackgroundHandler&) = delete;
  NtpBackgroundHandler& operator=(const NtpBackgroundHandler&) = delete;

  ~NtpBackgroundHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // Callbacks for JS APIs.
  void HandleClearBackground(const base::Value::List& args);
  void HandleGetBackgrounds(const base::Value::List& args);
  void HandleSetBackground(const base::Value::List& args);
};

}  // namespace welcome

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_NTP_BACKGROUND_HANDLER_H_
