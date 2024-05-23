// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_SIGNIN_NOTIFIER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_SIGNIN_NOTIFIER_H_

namespace password_manager {

class PasswordReuseManager;

// Abstract class for notifying PasswordReuseManager about sign-in events.
class PasswordReuseManagerSigninNotifier {
 public:
  PasswordReuseManagerSigninNotifier() = default;
  virtual ~PasswordReuseManagerSigninNotifier() = default;

  PasswordReuseManagerSigninNotifier(
      const PasswordReuseManagerSigninNotifier&) = delete;
  PasswordReuseManagerSigninNotifier& operator=(
      const PasswordReuseManagerSigninNotifier&) = delete;

  virtual void SubscribeToSigninEvents(PasswordReuseManager* reuse_manager) = 0;
  virtual void UnsubscribeFromSigninEvents() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_SIGNIN_NOTIFIER_H_
