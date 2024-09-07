// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_LOGIN_STATE_LOGIN_STATE_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_LOGIN_STATE_LOGIN_STATE_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/user_manager/user_manager.h"

namespace ash {

// Tracks the login state of chrome, accessible to Ash and other chromeos code.
class COMPONENT_EXPORT(LOGIN_STATE) LoginState
    : public user_manager::UserManager::UserSessionStateObserver {
 public:
  enum LoggedInState {
    LOGGED_IN_NONE,       // Not logged in
    LOGGED_IN_SAFE_MODE,  // Not logged in and login not allowed for non-owners
    LOGGED_IN_ACTIVE      // A user has logged in
  };

  enum LoggedInUserType {
    LOGGED_IN_USER_NONE,     // User is not logged in
    LOGGED_IN_USER_REGULAR,  // A regular user is logged in
    LOGGED_IN_USER_GUEST,    // A guest is logged in (i.e. incognito)
    // A user is logged in to a managed guest session ("Public Session v2").
    LOGGED_IN_USER_PUBLIC_ACCOUNT,
    LOGGED_IN_USER_KIOSK,  // Is in one of the kiosk modes -- Chrome App,
                           // Arc or Web App
    LOGGED_IN_USER_CHILD   // A child is logged in
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

  LoginState(const LoginState&) = delete;
  LoginState& operator=(const LoginState&) = delete;

  // Add/remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

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

  // Returns true if logged in to a managed guest session.
  bool IsManagedGuestSessionUser() const;

  // Returns true if logged in as a kiosk session.
  bool IsKioskSession() const;

  // Returns true if a child user is logged in.
  bool IsChildUser() const;

  // Whether a network profile is created for the user.
  bool UserHasNetworkProfile() const;

  // Returns true if the user is an authenticated user (i.e. the user is not
  // using an anonymous session like public or guest session)
  bool IsUserAuthenticated() const;

  void set_always_logged_in(bool always_logged_in) {
    always_logged_in_ = always_logged_in;
  }

  // DEPRECATED: please use
  // user_manager::UserManager::Get()->GetPrimaryUser()->username_hash().
  // TODO(b/278643115): Remove this.
  const std::string& primary_user_hash() const;

  void OnUserManagerCreated(user_manager::UserManager* user_manager);
  void OnUserManagerWillBeDestroyed(user_manager::UserManager* user_manager);

  // user_manager::UserManager::UserSessionStateObserver:
  void OnLoginStateUpdated(const user_manager::User* active_user) override;

 private:
  LoginState();
  ~LoginState() override;

  void NotifyObservers();

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::UserSessionStateObserver>
      observation_{this};

  LoggedInState logged_in_state_;
  LoggedInUserType logged_in_user_type_;
  std::string primary_user_hash_;
  base::ObserverList<Observer>::Unchecked observer_list_;

  // If true, it always thinks the current status as logged in. Set to true by
  // default running on a Linux desktop without flags and test cases. To test
  // behaviors with a specific login state, call set_always_logged_in(false).
  bool always_logged_in_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_LOGIN_STATE_LOGIN_STATE_H_
