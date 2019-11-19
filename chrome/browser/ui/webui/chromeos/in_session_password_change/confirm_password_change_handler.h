// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_CONFIRM_PASSWORD_CHANGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_CONFIRM_PASSWORD_CHANGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/saml/in_session_password_change_manager.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

class ConfirmPasswordChangeHandler
    : public content::WebUIMessageHandler,
      public InSessionPasswordChangeManager::Observer {
 public:
  ConfirmPasswordChangeHandler(const std::string& scraped_old_password,
                               const std::string& scraped_new_password,
                               const bool show_spinner_initially);
  ~ConfirmPasswordChangeHandler() override;

  // Called by the JS UI to find out what to show and what size to be.
  void HandleGetInitialState(const base::ListValue* params);

  // Tries to change the cryptohome password once the confirm-password-change
  // dialog is filled in and the password change is confirmed.
  void HandleChangePassword(const base::ListValue* passwords);

  // InSessionPasswordChangeManager::Observer:
  void OnEvent(InSessionPasswordChangeManager::Event event) override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  std::string scraped_old_password_;
  std::string scraped_new_password_;
  bool show_spinner_initially_ = false;

  base::WeakPtrFactory<ConfirmPasswordChangeHandler> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ConfirmPasswordChangeHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_CONFIRM_PASSWORD_CHANGE_HANDLER_H_
