// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_LOGIN_STATE_H_
#define CHROMEOS_LOGIN_LOGIN_STATE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// Tracks the login state of chrome, accessible to Ash and other chromeos code.
class CHROMEOS_EXPORT LoginState {
 public:
  enum LoggedInState {
    LOGGED_IN_NONE,       // Not logged in
    LOGGED_IN_SAFE_MODE,  // Not logged in and login not allowed for non-owners
    LOGGED_IN_ACTIVE      // A user has logged in
  };

  enum LoggedInUserType {
    LOGGED_IN_USER_NONE,            // User is not logged in
    LOGGED_IN_USER_REGULAR,         // A regular user is logged in
    LOGGED_IN_USER_OWNER,           // The owner of the device is logged in
    LOGGED_IN_USER_GUEST,           // A guest is logged in (i.e. incognito)
    LOGGED_IN_USER_PUBLIC_ACCOUNT,  // A user is logged in to a public session.
    LOGGED_IN_USER_PUBLIC_ACCOUNT_MANAGED,  // Public session v2.
    LOGGED_IN_USER_SUPERVISED,      // A supervised user is logged in
    LOGGED_IN_USER_KIOSK_APP,       // Is in kiosk app mode
    LOGGED_IN_USER_ARC_KIOSK_APP,   // Is in ARC kiosk mode
    LOGGED_IN_USER_CHILD            // A child is logged in
  };

  class Observer {
   public:
    // Called when either the login state or the logged in user type changes.
    virtual void LoggedInStateChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Manage singleton instance.
  static void Initialize();
  static void Shutdown();
  static LoginState* Get();
  static bool IsInitialized();

  // Add/remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sets the logged in state, user type, and primary user hash when the
  // primary user initialy logs in. Also notifies observers.
  void SetLoggedInStateAndPrimaryUser(
      LoggedInState state,
      LoggedInUserType type,
      const std::string& primary_user_hash);

  // Sets the logged in state and user type. Also notifies observers. Used
  // in tests or situations where there is no primary user (e.g. from the
  // login screen).
  void SetLoggedInState(LoggedInState state, LoggedInUserType type);

  // Gets the logged in user type.
  LoggedInUserType GetLoggedInUserType() const;

  // Returns true if a user is considered to be logged in.
  bool IsUserLoggedIn() const;

  // Returns true if |logged_in_state_| is safe mode (i.e. the user is not yet
  // logged in, and only the owner will be allowed to log in).
  bool IsInSafeMode() const;

  // Returns true if logged in to a guest session.
  bool IsGuestSessionUser() const;

  // Returns true if logged in to a public session.
  bool IsPublicSessionUser() const;

  // Returns true if restrictions should be enabled for public session.
  bool ArePublicSessionRestrictionsEnabled() const;

  // Returns true if logged in as a kiosk app.
  bool IsKioskApp() const;

  // Returns true if a child user is logged in.
  bool IsChildUser() const;

  // Whether a network profile is created for the user.
  bool UserHasNetworkProfile() const;

  // Returns true if the user is an authenticated user (i.e. the user is not
  // using an anonymous session like public or guest session)
  bool IsUserAuthenticated() const;

  // Returns true if the user is authenticated by logging into Google account
  // (i.e. not using an anonymous nor supervised session).
  bool IsUserGaiaAuthenticated() const;

  void set_always_logged_in(bool always_logged_in) {
    always_logged_in_ = always_logged_in;
  }

  const std::string& primary_user_hash() const { return primary_user_hash_; }

 private:
  LoginState();
  virtual ~LoginState();

  void NotifyObservers();

  LoggedInState logged_in_state_;
  LoggedInUserType logged_in_user_type_;
  std::string primary_user_hash_;
  base::ObserverList<Observer>::Unchecked observer_list_;

  // If true, it always thinks the current status as logged in. Set to true by
  // default running on a Linux desktop without flags and test cases. To test
  // behaviors with a specific login state, call set_always_logged_in(false).
  bool always_logged_in_;

  DISALLOW_COPY_AND_ASSIGN(LoginState);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_LOGIN_STATE_H_
