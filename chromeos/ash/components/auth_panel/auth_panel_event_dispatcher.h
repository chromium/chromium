// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_PANEL_EVENT_DISPATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_PANEL_EVENT_DISPATCHER_H_

#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

// This class is responsible for dispatching auth panel ui events to
// `AuthFactorStore`.
class AuthPanelEventDispatcher {
 public:
  enum UserAction {
    // Emitted whenever the user presses the pin/password toggle button.
    kPasswordPinToggle,
  };

  enum AuthVerdict {
    kSuccess,
    kFailure,
  };

  AuthPanelEventDispatcher();
  ~AuthPanelEventDispatcher();
  AuthPanelEventDispatcher(const AuthPanelEventDispatcher&) = delete;
  AuthPanelEventDispatcher(AuthPanelEventDispatcher&&) = delete;
  AuthPanelEventDispatcher& operator=(const AuthPanelEventDispatcher&) = delete;
  AuthPanelEventDispatcher& operator=(AuthPanelEventDispatcher&&) = delete;

  void DispatchEvent(AshAuthFactor factor, UserAction action);
  void DispatchEvent(AshAuthFactor factor, AuthVerdict verdict);
  void DispatchEvent(AshAuthFactor factor, AuthFactorState state);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_PANEL_EVENT_DISPATCHER_H_
