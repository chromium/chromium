// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_H_
#define COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/session_manager_export.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"

namespace session_manager {

class Session;
class SessionManagerObserver;

class SESSION_EXPORT SessionManager
    : public user_manager::UserManager::Observer {
 public:
  SessionManager();

  SessionManager(const SessionManager&) = delete;
  SessionManager& operator=(const SessionManager&) = delete;

  ~SessionManager() override;

  // Returns current SessionManager instance and NULL if it hasn't been
  // initialized yet.
  static SessionManager* Get();

  void SetSessionState(SessionState state);

  // Creates a session for the given user, hash and the type.
  // This is used for common session starts, and recovery from crash
  // for the secondary+ users. For the latter case, `has_active_session`
  // is set true.
  void CreateSession(const AccountId& user_account_id,
                     const std::string& username_hash,
                     bool new_user,
                     bool has_active_session);

  // Similar to above, creates a session for the given user and hash,
  // but for the primary user session on restarting chrome for crash recovering.
  // (Note: for non primary user sessions, CreateSession() is called with
  // `has_active_session == true`).
  // For this case, we expect there already is a registered User, so in general
  // the user type should be derived from the one. Though, there are edge
  // cases. Please find UserManager::CalculateUserType() for details.
  void CreateSessionForRestart(const AccountId& user_account_id,
                               const std::string& user_id_hash,
                               bool new_user);

  // Switches the active user session to the one specified by `account_id`.
  // The User has to be logged in already (i.e. CreateSession* needs to be
  // called in advance).
  void SwitchActiveSession(const AccountId& account_id);

  // Returns true if we're logged in and browser has been started i.e.
  // browser_creator.LaunchBrowser(...) was called after sign in
  // or restart after crash.
  bool IsSessionStarted() const;

  // Returns true if user session start up tasks are completed.
  bool IsUserSessionStartUpTaskCompleted() const;

  // Currently, UserManager is created after SessionManager.
  // However, UserManager is destroyed after SessionManager in the production.
  // Tests need to follow the same lifetime management.
  // TODO(b:332481586): Move this to the constructor by fixing initialization
  // order.
  virtual void OnUserManagerCreated(user_manager::UserManager* user_manager);

  // Called when browser session is started i.e. after
  // browser_creator.LaunchBrowser(...) was called after user sign in.
  // When user is at the image screen IsUserLoggedIn() will return true
  // but IsSessionStarted() will return false. During the kiosk splash screen,
  // we perform additional initialization after the user is logged in but
  // before the session has been started.
  virtual void SessionStarted();

  // Returns true if the session for the given user was started.
  bool HasSessionForAccountId(const AccountId& user_account_id) const;

  // Convenience wrapps of session state.
  bool IsInSecondaryLoginScreen() const;
  bool IsScreenLocked() const;
  bool IsUserSessionBlocked() const;

  void AddObserver(SessionManagerObserver* observer);
  void RemoveObserver(SessionManagerObserver* observer);

  void HandleUserSessionStartUpTaskCompleted();

  // user_manager::UserManager::Observer:
  void OnUserProfileCreated(const user_manager::User& user) override;

  // Various helpers to notify observers.
  void NotifyUserProfileLoaded(const AccountId& account_id);
  void NotifyNetworkErrorScreenShown();
  void NotifyLoginOrLockScreenVisible();
  void NotifyUnlockAttempt(const bool success, const UnlockType unlock_type);

  SessionState session_state() const { return session_state_; }
  const std::vector<std::unique_ptr<Session>>& sessions() const {
    return sessions_;
  }

  bool login_or_lock_screen_shown_for_test() const {
    return login_or_lock_screen_shown_for_test_;
  }

 protected:
  user_manager::UserManager* user_manager() { return user_manager_.get(); }

  // Called when a session is created. Make it possible for subclasses to inject
  // their more specific behavior at the timing.
  // TODO(crbug.com/278643115): Consolidate the subclass behaviors to this class
  // or extract into one of SessionManagerObserver's implementation.
  virtual void OnSessionCreated(bool browser_restart) {}

  // Sets SessionManager instance.
  static void SetInstance(SessionManager* session_manager);

 private:
  void CreateSessionInternal(const AccountId& user_account_id,
                             const std::string& username_hash,
                             bool new_user,
                             bool browser_restart);

  // Pointer to the existing SessionManager instance (if any).
  // Set in ctor, reset in dtor. Not owned since specific implementation of
  // SessionManager should decide on its own appropriate owner of SessionManager
  // instance. For src/chrome implementation such place is
  // g_browser_process->platform_part().
  static SessionManager* instance;

  raw_ptr<user_manager::UserManager> user_manager_ = nullptr;
  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observation_{this};

  SessionState session_state_ = SessionState::UNKNOWN;

  // ID of the user just added to the session that needs to be activated
  // as soon as user's profile is loaded.
  AccountId pending_active_account_id_ = EmptyAccountId();

  // True if SessionStarted() has been called.
  bool session_started_ = false;

  // True if HandleUserSessionStartUpTaskCompleted() has been called.
  bool user_session_start_up_task_completed_ = false;

  // True if `NotifyLoginOrLockScreenVisible()` has been called. Used by test
  // classes to determine whether they should observe the session manager, as
  // the session manager may not be available when the test object is created.
  bool login_or_lock_screen_shown_for_test_ = false;

  // Id of the primary session, i.e. the first user session.
  static constexpr SessionId kPrimarySessionId = 1;

  // ID assigned to the next session.
  SessionId next_id_ = kPrimarySessionId;

  // Keeps track of user sessions.
  std::vector<std::unique_ptr<Session>> sessions_;

  base::ObserverList<SessionManagerObserver> observers_;
};

}  // namespace session_manager

#endif  // COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_H_
