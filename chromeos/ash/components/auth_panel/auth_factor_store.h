// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_FACTOR_STORE_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_FACTOR_STORE_H_

#include <string>

#include "ash/ime/ime_controller_impl.h"
#include "ash/shell.h"
#include "base/callback_list.h"
#include "chromeos/ash/components/auth_panel/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// This class encapsulates the UI state of `AuthPanel`.
class AuthFactorStore {
 public:
  struct State {
    enum class AuthenticationStage {
      kIdle,
      kAuthenticating,
      kAuthenticated,
      kMaxValue = kAuthenticated,
    };

    struct PasswordViewState {
      bool is_display_password_button_visible_ = true;
      bool is_password_visible_ = false;
      bool is_factor_enabled_ = true;
      bool is_capslock_on_ = false;
      bool is_capslock_icon_highlighted_ = false;
      std::string password_;

      explicit PasswordViewState(bool is_capslock_on);
      ~PasswordViewState();
    };

    State();
    ~State();

    void InitializePasswordViewState(bool is_capslock_on);

    AuthenticationStage authentication_stage_ = AuthenticationStage::kIdle;
    absl::optional<PasswordViewState> password_view_state_;
  };

  using OnStateUpdatedCallback =
      base::RepeatingCallback<void(const State& state)>;
  using OnStateUpdatedCallbackList =
      base::RepeatingCallbackList<void(const State& state)>;

  explicit AuthFactorStore(Shell* shell);
  ~AuthFactorStore();

  base::CallbackListSubscription Subscribe(OnStateUpdatedCallback callback);

  void OnUserAction(const AuthPanelEventDispatcher::UserAction& action);
  void OnFactorStateChanged(AshAuthFactor factor, AuthFactorState state);
  void OnAuthVerdict(AshAuthFactor factor,
                     AuthPanelEventDispatcher::AuthVerdict verdict);

 private:
  void NotifyStateChanged();

  State state_;

  OnStateUpdatedCallbackList state_update_callbacks_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_AUTH_FACTOR_STORE_H_
