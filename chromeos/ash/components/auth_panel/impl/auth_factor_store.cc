// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/impl/auth_factor_store.h"

#include "ash/public/cpp/ime_controller.h"
#include "base/callback_list.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/osauth/public/auth_engine_api.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

AuthFactorStore::State::State() = default;

AuthFactorStore::State::~State() = default;

AuthFactorStore::State::PasswordViewState::PasswordViewState(
    bool is_capslock_on)
    : is_capslock_on_(is_capslock_on) {}

void AuthFactorStore::State::InitializePasswordViewState(bool is_capslock_on) {
  password_view_state_.emplace(is_capslock_on);
}

AuthFactorStore::State::PasswordViewState::~PasswordViewState() = default;

AuthFactorStore::AuthFactorStore(ImeController* ime_controller,
                                 AuthHubConnector* connector,
                                 std::optional<AshAuthFactor> password_type,
                                 AuthHub* auth_hub)
    : auth_hub_connector_(connector), auth_hub_(auth_hub) {
  password_type_ = password_type;

  // For now, assume the password view state is always required to be present
  // because we always have a password. We default to `false` for the state of
  // capslock if `ime_controller` is not available.
  state_.InitializePasswordViewState(
      ime_controller == nullptr ? false : ime_controller->IsCapsLockEnabled());

  submit_password_callback_ =
      base::BindRepeating(&AuthEngineApi::AuthenticateWithPassword);
}

AuthFactorStore::~AuthFactorStore() = default;

base::CallbackListSubscription AuthFactorStore::Subscribe(
    OnStateUpdatedCallback callback) {
  return state_update_callbacks_.Add(callback);
}

void AuthFactorStore::OnUserAction(
    const AuthPanelEventDispatcher::UserAction& action) {
  switch (action.type_) {
    case AuthPanelEventDispatcher::UserAction::kPasswordPinToggle: {
      NOTIMPLEMENTED();
      break;
    }
    case AuthPanelEventDispatcher::UserAction::kPasswordSubmit: {
      CHECK(state_.password_view_state_.has_value());

      auto password =
          state_.password_view_state_->auth_textfield_state_.password_;
      if (!password.empty() &&
          state_.password_view_state_->is_factor_enabled_) {
        state_.authentication_stage_ =
            State::AuthenticationStage::kAuthenticating;
        SubmitPassword(password);
      } else {
        NOTREACHED_IN_MIGRATION()
            << "AuthPanel: Password was submitted while textfield is "
            << "empty or password factor is disabled";
      }

      break;
    }
    case AuthPanelEventDispatcher::UserAction::kDisplayPasswordButtonPressed: {
      CHECK(state_.password_view_state_.has_value());

      bool& previous = state_.password_view_state_->auth_textfield_state_
                           .is_password_visible_;
      previous = !previous;

      break;
    }
    case AuthPanelEventDispatcher::UserAction::
        kPasswordTextfieldContentsChanged: {
      CHECK(action.payload_.has_value());
      CHECK(state_.password_view_state_.has_value());

      auto new_field_contents = *action.payload_;
      state_.password_view_state_->auth_textfield_state_.password_ =
          new_field_contents;
      break;
    }
    case AuthPanelEventDispatcher::UserAction::kCapslockKeyPressed: {
      CHECK(state_.password_view_state_.has_value());

      state_.password_view_state_->is_capslock_on_ =
          !state_.password_view_state_->is_capslock_on_;
      break;
    }
    case AuthPanelEventDispatcher::UserAction::kPasswordTextfieldFocused: {
      CHECK(state_.password_view_state_.has_value());

      state_.password_view_state_->is_password_textfield_focused_ = true;
      break;
    }
    case AuthPanelEventDispatcher::UserAction::kPasswordTextfieldBlurred: {
      CHECK(state_.password_view_state_.has_value());

      state_.password_view_state_->is_password_textfield_focused_ = false;
      break;
    }
    case AuthPanelEventDispatcher::UserAction::
        kEscapePressedOnPasswordTextfield: {
      CHECK(state_.password_view_state_.has_value());

      auth_hub_->CancelCurrentAttempt(auth_hub_connector_);
      break;
    }
  }

  NotifyStateChanged();
}

void AuthFactorStore::OnFactorStateChanged(AshAuthFactor factor,
                                           AuthFactorState state) {
  switch (factor) {
    case AshAuthFactor::kGaiaPassword:
    case AshAuthFactor::kLocalPassword:
      CHECK(password_type_.has_value());
      CHECK_EQ(factor, password_type_.value());

      state_.password_view_state_->factor_state_ = state;
      break;
    case AshAuthFactor::kCryptohomePin:
    case AshAuthFactor::kFingerprint:
    case AshAuthFactor::kSmartCard:
    case AshAuthFactor::kSmartUnlock:
    case AshAuthFactor::kRecovery:
    case AshAuthFactor::kLegacyPin:
    case AshAuthFactor::kLegacyFingerprint:
      NOTIMPLEMENTED();
      break;
  }

  NotifyStateChanged();
}

void AuthFactorStore::OnAuthVerdict(
    AshAuthFactor factor,
    AuthPanelEventDispatcher::AuthVerdict verdict) {
  NOTIMPLEMENTED();
}

void AuthFactorStore::NotifyStateChanged() {
  state_update_callbacks_.Notify(state_);
}

void AuthFactorStore::SubmitPassword(const std::string& password) {
  if (state_.password_view_state_->factor_state_ !=
      AuthFactorState::kFactorReady) {
    LOG(ERROR) << "Attempting to submit password while password factor state "
                  "is not ready";
    return;
  }

  // We cannot reach here if the password factor was unavailable, as the UI
  // for it would not have been shown. Check this invariant here.
  CHECK(password_type_.has_value());

  submit_password_callback_.Run(auth_hub_connector_, password_type_.value(),
                                password);
}

}  // namespace ash
