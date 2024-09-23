// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_H_
#define COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "components/session_manager/session_manager_export.h"
#include "components/session_manager/session_manager_types.h"

class AccountId;

namespace session_manager {

class SessionManagerObserver;

class SESSION_EXPORT SessionManager {
 public:
  SessionManager();

  SessionManager(const SessionManager&) = delete;
  SessionManager& operator=(const SessionManager&) = delete;

  virtual ~SessionManager();

  // Returns current SessionManager instance and NULL if it hasn't been
  // initialized yet.
  static SessionManager* Get();

  void SetSessionState(SessionState state);

  // Creates a session for the given user. The first one is for regular cases
  // and the 2nd one is for the crash-and-restart case.
  void CreateSession(const AccountId& user_account_id,
                     const std::string& user_id_hash,
                     bool is_child);
  void CreateSessionForRestart(const AccountId& user_account_id,
                               const std::string& user_id_hash);

  // Returns true if we're logged in and browser has been started i.e.
  // browser_creator.LaunchBrowser(...) was called after sign in
  // or restart after crash.
  bool IsSessionStarted() const;

  // Returns true if user session start up tasks are completed.
  bool IsUserSessionStartUpTaskCompleted() const;

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

  // Various helpers to notify observers.
  void NotifyUserProfileLoaded(const AccountId& account_id);
  void NotifyNetworkErrorScreenShown();
  void NotifyLoginOrLockScreenVisible();
  void NotifyUnlockAttempt(const bool success, const UnlockType unlock_type);

  SessionState session_state() const { return session_state_; }
  const std::vector<Session>& sessions() const { return sessions_; }

  bool login_or_lock_screen_shown_for_test() const {
    return login_or_lock_screen_shown_for_test_;
  }

 protected:
  // Notifies UserManager about a user signs in when creating a user session.
  virtual void NotifyUserLoggedIn(const AccountId& user_account_id,
                                  const std::string& user_id_hash,
                                  bool browser_restart,
                                  bool is_child);

  // Sets SessionManager instance.
  static void SetInstance(SessionManager* session_manager);

 private:
  void CreateSessionInternal(const AccountId& user_account_id,
                             const std::string& user_id_hash,
                             bool browser_restart,
                             bool is_child);

  // Pointer to the existing SessionManager instance (if any).
  // Set in ctor, reset in dtor. Not owned since specific implementation of
  // SessionManager should decide on its own appropriate owner of SessionManager
  // instance. For src/chrome implementation such place is
  // g_browser_process->platform_part().
  static SessionManager* instance;

  SessionState session_state_ = SessionState::UNKNOWN;

  // True if SessionStarted() has been called.
  bool session_started_ = false;

  // True if HandleUserSessionStartUpTaskCompleted() has been called.
  bool user_session_start_up_task_completed_ = false;

  // True if `NotifyLoginOrLockScreenVisible()` has been called. Used by test
  // classes to determine whether they should observe the session manager, as
  // the session manager may not be available when the test object is created.
  bool login_or_lock_screen_shown_for_test_ = false;

  // Id of the primary session, i.e. the first user session.
  static const SessionId kPrimarySessionId = 1;

  // ID assigned to the next session.
  SessionId next_id_ = kPrimarySessionId;

  // Keeps track of user sessions.
  std::vector<Session> sessions_;

  base::ObserverList<SessionManagerObserver> observers_;
};

}  // namespace session_manager

#endif  // COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_H_
