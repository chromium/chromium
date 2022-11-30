// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SIGNIN_NOTIFIER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SIGNIN_NOTIFIER_H_

namespace password_manager {

class PasswordReuseManager;

// Abstract class for notifying PasswordStore about sign-in events.
// TODO(crbug.bom/715987): Rename into PasswordReuseManagerSigninNotifier.
class PasswordStoreSigninNotifier {
 public:
  PasswordStoreSigninNotifier() = default;
  virtual ~PasswordStoreSigninNotifier() = default;

  PasswordStoreSigninNotifier(const PasswordStoreSigninNotifier&) = delete;
  PasswordStoreSigninNotifier& operator=(const PasswordStoreSigninNotifier&) =
      delete;

  virtual void SubscribeToSigninEvents(PasswordReuseManager* reuse_manager) = 0;
  virtual void UnsubscribeFromSigninEvents() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SIGNIN_NOTIFIER_H_
