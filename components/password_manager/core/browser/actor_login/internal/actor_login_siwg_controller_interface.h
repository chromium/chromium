// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_SIWG_CONTROLLER_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_SIWG_CONTROLLER_INTERFACE_H_

#include <memory>

#include "components/password_manager/core/browser/actor_login/internal/actor_login_metrics_helper.h"

namespace actor_login {

// Interface for the controller for Sign-in with Google interaction.
class ActorLoginSiwgControllerInterface {
 public:
  ActorLoginSiwgControllerInterface() = default;
  virtual ~ActorLoginSiwgControllerInterface() = default;

  // Not copyable or movable.
  ActorLoginSiwgControllerInterface(const ActorLoginSiwgControllerInterface&) =
      delete;
  ActorLoginSiwgControllerInterface& operator=(
      const ActorLoginSiwgControllerInterface&) = delete;

  virtual bool ShouldStorePermission() const = 0;

  // Starts the federated login flow. This will notify FedCM API that an
  // automated login is in progress, and then start the button detection and
  // click flow.
  virtual void StartFederatedLogin(
      std::unique_ptr<ActorLoginMetricsHelper> metrics_helper) = 0;
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_SIWG_CONTROLLER_INTERFACE_H_
