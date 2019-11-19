// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_NTP_BACKGROUND_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_NTP_BACKGROUND_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace welcome {

class NtpBackgroundHandler : public content::WebUIMessageHandler {
 public:
  NtpBackgroundHandler();
  ~NtpBackgroundHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // Callbacks for JS APIs.
  void HandleClearBackground(const base::ListValue* args);
  void HandleGetBackgrounds(const base::ListValue* args);
  void HandleSetBackground(const base::ListValue* args);

 private:
  DISALLOW_COPY_AND_ASSIGN(NtpBackgroundHandler);
};

}  // namespace welcome

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_NTP_BACKGROUND_HANDLER_H_
