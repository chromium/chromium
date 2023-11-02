// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash {

class PasswordChangeHandler : public content::WebUIMessageHandler {
 public:
  explicit PasswordChangeHandler(const std::string& password_change_url);

  PasswordChangeHandler(const PasswordChangeHandler&) = delete;
  PasswordChangeHandler& operator=(const PasswordChangeHandler&) = delete;

  ~PasswordChangeHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  void HandleInitialize(const base::Value::List&);
  void HandleChangePassword(const base::Value::List& passwords);

 private:
  const std::string password_change_url_;

  base::WeakPtrFactory<PasswordChangeHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_IN_SESSION_PASSWORD_CHANGE_PASSWORD_CHANGE_HANDLER_H_
