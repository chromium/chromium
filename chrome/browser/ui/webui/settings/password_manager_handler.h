// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PASSWORD_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PASSWORD_MANAGER_HANDLER_H_

#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace settings {

// A which enables interactions with Password Manager from settings.
class PasswordManagerHandler : public content::WebUIMessageHandler {
 public:
  explicit PasswordManagerHandler();

  PasswordManagerHandler(const PasswordManagerHandler&) = delete;
  PasswordManagerHandler& operator=(const PasswordManagerHandler&) = delete;

  ~PasswordManagerHandler() override;

 private:
  // WebUIMessageHandler:
  void RegisterMessages() override;

  void HandleShowPasswordManager(const base::Value::List& args);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PASSWORD_MANAGER_HANDLER_H_
