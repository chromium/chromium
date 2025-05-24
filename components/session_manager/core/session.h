// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_CORE_SESSION_H_
#define COMPONENTS_SESSION_MANAGER_CORE_SESSION_H_

#include "components/account_id/account_id.h"
#include "components/session_manager/session_manager_export.h"
#include "components/session_manager/session_manager_types.h"

namespace session_manager {

// Represents a session.
class SESSION_EXPORT Session {
 public:
  Session(SessionId session_id, const AccountId& account_id);
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  ~Session();

  // Returns the ID for this session.
  SessionId session_id() const { return session_id_; }

  // Returns the AccountId for the User of this session.
  const AccountId& account_id() const { return account_id_; }

 private:
  const SessionId session_id_;

  // TODO(crbug.com/278643115): Replace with user_manager::User.
  const AccountId account_id_;
};

}  // namespace session_manager

#endif  // COMPONENTS_SESSION_MANAGER_CORE_SESSION_H_
