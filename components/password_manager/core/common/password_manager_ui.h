// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UI_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UI_H_

namespace password_manager {

namespace ui {

// The current state of the password manager's UI.
enum State {
  // The password manager has nothing to do with the current site.
  INACTIVE_STATE,

  // A password is pending.
  PENDING_PASSWORD_STATE,

  // A password has been saved and we wish to display UI confirming the save
  // to the user.
  CONFIRMATION_STATE,

  // A password has been autofilled, or has just been saved. The icon needs
  // to be visible, in the management state.
  MANAGE_STATE,

  // The site has asked user to choose a credential.
  CREDENTIAL_REQUEST_STATE,

  // The user was auto signed in to the site. The icon and the auto-signin toast
  // should be visible.
  AUTO_SIGNIN_STATE,

  // The user submitted a form that we consider to be a change password form.
  // Chrome needs to ask the user to confirm password updating.
  PENDING_PASSWORD_UPDATE_STATE,

  // A user opted in to account storage is about to lose some unsynced
  // passwords.
  WILL_DELETE_UNSYNCED_ACCOUNT_PASSWORDS_STATE,

  // The user used a profile credential to log in successfully and should see a
  // prompt that allows them to move the credential to their account store.
  CAN_MOVE_PASSWORD_TO_ACCOUNT_STATE,

  // Last compromised password was updated and the user is safe.
  PASSWORD_UPDATED_SAFE_STATE,

  // A compromised password was updated and the user has more to fix.
  PASSWORD_UPDATED_MORE_TO_FIX,
};

}  // namespace ui

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UI_H_
