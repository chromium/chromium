// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_AUTH_FACTOR_STORE_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_AUTH_FACTOR_STORE_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/auth_panel/public/shared_types.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

class AuthHubConnector;
class AuthPanel;
class ImeController;

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

    struct AuthTextfieldState {
      bool is_password_visible_ = false;
      bool is_read_only = false;
      std::string password_;
    };

    struct PasswordViewState {
      bool is_display_password_button_visible_ = true;
      bool is_factor_enabled_ = true;
      bool is_capslock_on_ = false;
      bool is_password_textfield_focused_ = true;
      AuthFactorState factor_state_;
      AuthTextfieldState auth_textfield_state_;

      explicit PasswordViewState(bool is_capslock_on);
      ~PasswordViewState();
    };

    State();
    ~State();

    void InitializePasswordViewState(bool is_capslock_on);
    void OnAshAuthFactorStateChanged(AshAuthFactor factor,
                                     AuthFactorState state);

    AuthenticationStage authentication_stage_ = AuthenticationStage::kIdle;
    std::optional<PasswordViewState> password_view_state_;
  };

  using OnStateUpdatedCallback =
      base::RepeatingCallback<void(const State& state)>;
  using OnStateUpdatedCallbackList =
      base::RepeatingCallbackList<void(const State& state)>;

  AuthFactorStore(ImeController* ime_controller,
                  AuthHubConnector* connector,
                  std::optional<AshAuthFactor> password_type,
                  AuthHub* auth_hub);
  ~AuthFactorStore();

  base::CallbackListSubscription Subscribe(OnStateUpdatedCallback callback);

  void OnUserAction(const AuthPanelEventDispatcher::UserAction& action);
  void OnFactorStateChanged(AshAuthFactor factor, AuthFactorState state);
  void OnAuthVerdict(AshAuthFactor factor,
                     AuthPanelEventDispatcher::AuthVerdict verdict);
 private:
  friend class AuthPanel::TestApi;

  void NotifyStateChanged();

  void SubmitPassword(const std::string& password);

  State state_;

  OnStateUpdatedCallbackList state_update_callbacks_;

  std::optional<AshAuthFactor> password_type_;

  raw_ptr<AuthHubConnector> auth_hub_connector_;

  auth_panel::SubmitPasswordCallback submit_password_callback_;

  raw_ptr<AuthHub> auth_hub_;
};

class AuthFactorStoreFactory {
 public:
  explicit AuthFactorStoreFactory(AuthHub* auth_hub) : auth_hub_(auth_hub) {}

  std::unique_ptr<AuthFactorStore> CreateAuthFactorStore(
      ImeController* ime_controller,
      AuthHubConnector* connector,
      std::optional<AshAuthFactor> password_type) {
    return std::make_unique<AuthFactorStore>(ime_controller, connector,
                                             password_type, auth_hub_);
  }

 private:
  // AuthHub is a long-lived, singleton object. It's created early in Ash's
  // lifecycle and destroyed late, after message loop stops. It is therefore
  // guaranteed to outlive `AuthFactorStoreFactory` and `AuthFactorStore`.
  raw_ptr<AuthHub> auth_hub_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_AUTH_FACTOR_STORE_H_
