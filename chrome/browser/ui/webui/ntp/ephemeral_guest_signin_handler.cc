// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ephemeral_guest_signin_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/signin/signin_util.h"

EphemeralGuestSigninHandler::EphemeralGuestSigninHandler(Profile* profile)
    : profile_(profile) {}
EphemeralGuestSigninHandler::~EphemeralGuestSigninHandler() = default;
// WebUIMessageHandler
void EphemeralGuestSigninHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "onChangeSignInStatusClicked",
      base::BindRepeating(
          &EphemeralGuestSigninHandler::HandleOnChangeSignInStatusClicked,
          base::Unretained(this)));
}

void EphemeralGuestSigninHandler::HandleOnChangeSignInStatusClicked(
    const base::ListValue* args) {
  IsSignedIn() ? SignOutAsGuest() : SignInAsGuest();
}

void EphemeralGuestSigninHandler::SignInAsGuest() {
  // TODO(crbug.com/1134111): Use IdentityManager to sign in when Ephemeral
  // Guest sign in functioncality is implemented.
  signin_util::GuestSignedInUserData::SetIsSignedIn(profile_,
                                                    /*is_signed_in*/ true);
}

void EphemeralGuestSigninHandler::SignOutAsGuest() {
  // TODO(crbug.com/1134111): Use IdentityManager to sign out when Ephemeral
  // Guest sign in functioncality is implemented.
  signin_util::GuestSignedInUserData::SetIsSignedIn(profile_,
                                                    /*is_signed_in*/ false);
}

bool EphemeralGuestSigninHandler::IsSignedIn() {
  // TODO(crbug.com/1134111): Use IdentityManager to check sign in status when
  // Ephemeral Guest sign in functioncality is implemented.
  return signin_util::GuestSignedInUserData::IsSignedIn(profile_);
}
