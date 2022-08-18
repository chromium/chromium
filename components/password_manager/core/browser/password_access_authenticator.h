// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_ACCESS_AUTHENTICATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_ACCESS_AUTHENTICATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/password_manager/core/browser/reauth_purpose.h"

namespace password_manager {

// This class takes care of reauthentication used for accessing passwords
// through the settings page. It is used on all platforms but iOS and Android
// (see //ios/chrome/browser/ui/settings/reauthentication_module.* for iOS and
// PasswordEntryEditor.java and PasswordReauthenticationFragment.java in
// chrome/android/java/src/org/chromium/chrome/browser/preferences/password/
// for Android).
class PasswordAccessAuthenticator {
 public:
  using AuthResultCallback = base::OnceCallback<void(bool)>;
  using ReauthCallback =
      base::RepeatingCallback<void(ReauthPurpose, AuthResultCallback)>;
  using TimeoutCallback = base::RepeatingCallback<void()>;

  // For how long after the last successful authentication a user is considered
  // authenticated without repeating the challenge.
  constexpr static base::TimeDelta kAuthValidityPeriod = base::Seconds(60);

  // |os_reauth_call| is passed to |os_reauth_call_|, see the latter for
  // explanation. |timeout_call| is passed to |timeout_call_| and will be called
  // when |auth_timer_| runs out.
  PasswordAccessAuthenticator(ReauthCallback os_reauth_call,
                              TimeoutCallback timeout_call);

  PasswordAccessAuthenticator(const PasswordAccessAuthenticator&) = delete;
  PasswordAccessAuthenticator& operator=(const PasswordAccessAuthenticator&) =
      delete;

  ~PasswordAccessAuthenticator();

  // Determines whether the user is able to pass the authentication challenge,
  // which is represented by |os_reauth_call_| returning true. A successful
  // result of |os_reauth_call_| is cached for kAuthValidityPeriod. The result
  // is passed to |callback|.
  void EnsureUserIsAuthenticated(ReauthPurpose purpose,
                                 AuthResultCallback callback);

  // Presents the reauthentication challenge to the user and returns whether
  // the user passed the challenge. This call is guaranteed to present the
  // challenge to the user. The result is passed to |callback|.
  void ForceUserReauthentication(ReauthPurpose purpose,
                                 AuthResultCallback callback);

#if defined(UNIT_TEST)
  // Use this in tests to mock the OS-level reauthentication.
  void set_os_reauth_call(ReauthCallback os_reauth_call) {
    os_reauth_call_ = std::move(os_reauth_call);
  }
#endif  // defined(UNIT_TEST)

 private:
  // Callback for ForceUserReauthentication().
  void OnUserReauthenticationResult(AuthResultCallback callback,
                                    bool authenticated);

  // Used to directly present the authentication challenge (such as the login
  // prompt) to the user.
  ReauthCallback os_reauth_call_;

  // Fired after `kAuthValidityPeriod` after successful user authentication.
  TimeoutCallback timeout_call_;

  // Used to keep track of time once the user passed the auth challenge. Once it
  // runs out, |timeout_call_| will be run.
  base::RetainingOneShotTimer auth_timer_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_ACCESS_AUTHENTICATOR_H_
