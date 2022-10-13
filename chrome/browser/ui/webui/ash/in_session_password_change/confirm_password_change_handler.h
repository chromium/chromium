// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_IN_SESSION_PASSWORD_CHANGE_CONFIRM_PASSWORD_CHANGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_IN_SESSION_PASSWORD_CHANGE_CONFIRM_PASSWORD_CHANGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash {

class ConfirmPasswordChangeHandler
    : public content::WebUIMessageHandler,
      public InSessionPasswordChangeManager::Observer {
 public:
  ConfirmPasswordChangeHandler(const std::string& scraped_old_password,
                               const std::string& scraped_new_password,
                               const bool show_spinner_initially);

  ConfirmPasswordChangeHandler(const ConfirmPasswordChangeHandler&) = delete;
  ConfirmPasswordChangeHandler& operator=(const ConfirmPasswordChangeHandler&) =
      delete;

  ~ConfirmPasswordChangeHandler() override;

  // Called by the JS UI to find out what to show and what size to be.
  void HandleGetInitialState(const base::Value::List& params);

  // Tries to change the cryptohome password once the confirm-password-change
  // dialog is filled in and the password change is confirmed.
  void HandleChangePassword(const base::Value::List& passwords);

  // InSessionPasswordChangeManager::Observer:
  void OnEvent(InSessionPasswordChangeManager::Event event) override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  std::string scraped_old_password_;
  std::string scraped_new_password_;
  bool show_spinner_initially_ = false;

  base::WeakPtrFactory<ConfirmPasswordChangeHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_IN_SESSION_PASSWORD_CHANGE_CONFIRM_PASSWORD_CHANGE_HANDLER_H_
