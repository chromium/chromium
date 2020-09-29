// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

class LockScreenReauthHandler : public content::WebUIMessageHandler {
 public:
  LockScreenReauthHandler();
  ~LockScreenReauthHandler() override;

  void RegisterMessages() override;

  void HandleInitialize(const base::ListValue*);

 private:
  base::WeakPtrFactory<LockScreenReauthHandler> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_REAUTH_HANDLER_H_
