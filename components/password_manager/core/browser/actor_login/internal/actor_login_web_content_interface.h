// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_WEB_CONTENT_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_WEB_CONTENT_INTERFACE_H_

namespace actor_login {

// Interface to handle WebContents/web-facing events (such as page navigation
// or destruction of the environment/tab) for Actor Login.
class ActorLoginWebContentInterface {
 public:
  virtual ~ActorLoginWebContentInterface() = default;

  // Called when the active page navigates.
  virtual void OnPrimaryPageChanged() = 0;

  // Called when the context/hosting environment is being destroyed.
  virtual void OnContextDestroyed() = 0;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_WEB_CONTENT_INTERFACE_H_
