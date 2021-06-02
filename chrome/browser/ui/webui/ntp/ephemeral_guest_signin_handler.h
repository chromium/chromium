// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_EPHEMERAL_GUEST_SIGNIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_EPHEMERAL_GUEST_SIGNIN_HANDLER_H_

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

// TODO(crbug.com/1125474): Rename to GuestSigninHandler once all audit is done
// and all instances of non-ephemeral Guest profiles are deprecated.
// Communicates with the ephemeral guest ntp to handle sign-in and sign-out.
class EphemeralGuestSigninHandler : public content::WebUIMessageHandler {
 public:
  explicit EphemeralGuestSigninHandler(Profile* profile);
  ~EphemeralGuestSigninHandler() override;

  // WebUIMessageHandler
  void RegisterMessages() override;

  // Resolves JS call to sign in or sign out based on the |profile_| sign in
  // status.
  void HandleOnChangeSignInStatusClicked(const base::ListValue*);

 private:
  // Helper methods to handle sign in and sign out.
  void SignInAsGuest();
  void SignOutAsGuest();
  bool IsSignedIn();

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(EphemeralGuestSigninHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_EPHEMERAL_GUEST_SIGNIN_HANDLER_H_
