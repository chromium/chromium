// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_OBSERVER_H_
#define COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_OBSERVER_H_

#include "base/macros.h"
#include "base/observer_list_types.h"
#include "components/session_manager/session_manager_types.h"

namespace session_manager {

// An observer interface for SessionManager.
// TODO(xiyuan): Use this to replace UserManager::UserSessionStateObserver,
//     http://crbug.com/657149.
class SessionManagerObserver : public base::CheckedObserver {
 public:
  // Invoked when session state is changed.
  virtual void OnSessionStateChanged() {}

  // Invoked when a user profile is loaded.
  virtual void OnUserProfileLoaded(const AccountId& account_id) {}

  // Invoked when a user session is started. If this is a new user on the
  // machine this will not be called until after post-login steps are finished
  // (for example a profile picture has been selected). In contrast,
  // UserSessionStateObserver::OnActiveUserChanged() is invoked immediately
  // after the user has logged in.
  virtual void OnUserSessionStarted(bool is_primary_user) {}
};

}  // namespace session_manager

#endif  // COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_OBSERVER_H_
