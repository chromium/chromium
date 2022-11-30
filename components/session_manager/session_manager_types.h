// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_SESSION_MANAGER_TYPES_H_
#define COMPONENTS_SESSION_MANAGER_SESSION_MANAGER_TYPES_H_

#include "components/account_id/account_id.h"

namespace session_manager {

// TODO(xiyuan): Get rid/consolidate with chromeos::LoggedInState.
enum class SessionState {
  // Default value, when session state hasn't been initialized yet.
  UNKNOWN = 0,

  // Running out of box UI.
  OOBE,

  // Running login UI (primary user), including the post login steps such as
  // selecting avatar, agreeing to terms of service etc.
  LOGIN_PRIMARY,

  // A transient state between LOGIN_PRIMARY/LOGIN_SECONDARY and ACTIVE to
  // prepare user desktop environment (e.g. launching browser windows) while the
  // login screen is still visible. Should only be set from OOBE,
  // LOGIN_PRIMARY, or LOGIN_SECONDARY.
  LOGGED_IN_NOT_ACTIVE,

  // A user(s) has logged in *and* login UI is hidden i.e. user session is
  // not blocked.
  ACTIVE,

  // The session screen is locked.
  LOCKED,

  // Same as LOGIN_PRIMARY but for multi-profiles sign in i.e. when there's at
  // least one user already active in the session.
  LOGIN_SECONDARY,

  // Device is being repaired and the RMA app is active.
  RMA,
};

// A type for session id.
using SessionId = int;

// Info about a user session.
struct Session {
  SessionId id;
  AccountId user_account_id;
};

// Limits the number of logged in users to 5. User-switcher UI was not designed
// around a large number of users. This also helps on memory-constrained
// devices. See b/64593342 for some additional context.
constexpr int kMaximumNumberOfUserSessions = 5;

// Type of unlock method used.
enum class UnlockType {
  PASSWORD,
  PIN,
  FINGERPRINT,
  CHALLENGE_RESPONSE,
  EASY_UNLOCK,
  UNKNOWN
};

}  // namespace session_manager

#endif  // COMPONENTS_SESSION_MANAGER_SESSION_MANAGER_TYPES_H_
