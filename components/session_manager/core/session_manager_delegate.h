// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_DELEGATE_H_
#define COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_DELEGATE_H_

namespace session_manager {

// Delegate for `SessionManager` to request actions from a higher layer.
// This allows the `SessionManager` component to remain decoupled from //chrome.
class SessionManagerDelegate {
 public:
  SessionManagerDelegate() = default;

  virtual ~SessionManagerDelegate() = default;

  // Requests to sign out the user from the current session.
  // This action attempts to shut down the Chrome process and may restart it
  // to complete the sign-out.
  virtual void RequestSignOut() = 0;
};

}  // namespace session_manager

#endif  // COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_DELEGATE_H_
