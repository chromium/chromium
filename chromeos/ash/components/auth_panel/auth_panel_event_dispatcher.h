// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_PANEL_EVENT_DISPATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_PANEL_EVENT_DISPATCHER_H_

#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/auth_panel/factor_auth_view.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class AuthFactorStore;

// This class is responsible for dispatching auth panel ui events to
// `AuthFactorStore`.
class AuthPanelEventDispatcher {
 public:
  struct UserAction {
    enum Type {
      // Emitted whenever the user presses the pin/password toggle button.
      kPasswordPinToggle,
      kMaxValue = kPasswordPinToggle,
    };

    UserAction();
    ~UserAction();
    UserAction(const UserAction& other) = delete;
    UserAction& operator=(const UserAction& other) = delete;
    UserAction(UserAction&& other) = delete;
    UserAction& operator=(UserAction&& other) = delete;

    Type type;
    absl::optional<std::string> value;
  };

  enum AuthVerdict {
    kSuccess,
    kFailure,
  };

  explicit AuthPanelEventDispatcher(base::raw_ptr<AuthFactorStore> store);
  ~AuthPanelEventDispatcher();
  AuthPanelEventDispatcher(const AuthPanelEventDispatcher&) = delete;
  AuthPanelEventDispatcher(AuthPanelEventDispatcher&&) = delete;
  AuthPanelEventDispatcher& operator=(const AuthPanelEventDispatcher&) = delete;
  AuthPanelEventDispatcher& operator=(AuthPanelEventDispatcher&&) = delete;

  void DispatchEvent(AshAuthFactor factor, const UserAction& action);
  void DispatchEvent(AshAuthFactor factor, AuthFactorState state);
  void DispatchEvent(AshAuthFactor factor, AuthVerdict verdict);

 private:
  base::raw_ptr<AuthFactorStore> store_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_PANEL_EVENT_DISPATCHER_H_
