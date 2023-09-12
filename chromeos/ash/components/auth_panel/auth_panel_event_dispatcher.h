// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_PANEL_EVENT_DISPATCHER_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_PANEL_EVENT_DISPATCHER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
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
      kPasswordPinToggle,
      kPasswordSubmit,
      kDisplayPasswordButtonPressed,
      kPasswordTextfieldContentsChanged,
      kCapslockKeyPressed,
      kPasswordTextfieldFocused,
      kPasswordTextfieldBlurred,
      kMaxValue = kPasswordTextfieldBlurred,
    };

    UserAction(Type type, absl::optional<std::string> payload);
    ~UserAction();

    Type type_;
    absl::optional<std::string> payload_;
  };

  enum AuthVerdict {
    kSuccess,
    kFailure,
  };

  explicit AuthPanelEventDispatcher(raw_ptr<AuthFactorStore> store);
  ~AuthPanelEventDispatcher();
  AuthPanelEventDispatcher(const AuthPanelEventDispatcher&) = delete;
  AuthPanelEventDispatcher(AuthPanelEventDispatcher&&) = delete;
  AuthPanelEventDispatcher& operator=(const AuthPanelEventDispatcher&) = delete;
  AuthPanelEventDispatcher& operator=(AuthPanelEventDispatcher&&) = delete;

  void DispatchEvent(const UserAction& action);
  void DispatchEvent(AshAuthFactor factor, AuthFactorState state);
  void DispatchEvent(AshAuthFactor factor, AuthVerdict verdict);

 private:
  raw_ptr<AuthFactorStore> store_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_PANEL_EVENT_DISPATCHER_H_
