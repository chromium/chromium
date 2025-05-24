// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/auth_factor_input.h"

#include <utility>
#include <variant>

#include "base/check_op.h"
#include "base/notreached.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"

namespace cryptohome {

// =============== `RecoveryCreation` =====================
AuthFactorInput::RecoveryCreation::RecoveryCreation(
    const std::string& pub_key,
    const GaiaId& user_gaia_id,
    const std::string& device_user_id,
    bool ensure_fresh_recovery_id)
    : pub_key(pub_key),
      user_gaia_id(user_gaia_id),
      device_user_id(device_user_id),
      ensure_fresh_recovery_id(ensure_fresh_recovery_id) {}
AuthFactorInput::RecoveryCreation::RecoveryCreation(
    const RecoveryCreation& other) = default;
AuthFactorInput::RecoveryCreation& AuthFactorInput::RecoveryCreation::operator=(
    const RecoveryCreation&) = default;
AuthFactorInput::RecoveryCreation::~RecoveryCreation() = default;

// =============== `SmartCard` =====================
AuthFactorInput::SmartCard::SmartCard(
    std::vector<ChallengeResponseKey::SignatureAlgorithm> algorithms,
    std::string dbus_service_name)
    : signature_algorithms(std::move(algorithms)),
      key_delegate_dbus_service_name(std::move(dbus_service_name)) {}
AuthFactorInput::SmartCard::SmartCard(const SmartCard& other) = default;
AuthFactorInput::SmartCard& AuthFactorInput::SmartCard::operator=(
    const SmartCard&) = default;
AuthFactorInput::SmartCard::~SmartCard() = default;

// =============== `AuthFactorInput` ===============

AuthFactorInput::AuthFactorInput(InputVariant input)
    : factor_input_(std::move(input)) {}

AuthFactorInput::AuthFactorInput(AuthFactorInput&&) noexcept = default;
AuthFactorInput& AuthFactorInput::operator=(AuthFactorInput&&) noexcept =
    default;

AuthFactorInput::~AuthFactorInput() = default;

AuthFactorType AuthFactorInput::GetType() const {
  if (std::holds_alternative<AuthFactorInput::Password>(factor_input_)) {
    return AuthFactorType::kPassword;
  }
  if (std::holds_alternative<AuthFactorInput::Pin>(factor_input_)) {
    return AuthFactorType::kPin;
  }
  if (std::holds_alternative<AuthFactorInput::Kiosk>(factor_input_)) {
    return AuthFactorType::kKiosk;
  }
  if (std::holds_alternative<AuthFactorInput::SmartCard>(factor_input_)) {
    return AuthFactorType::kSmartCard;
  }
  if (std::holds_alternative<AuthFactorInput::RecoveryCreation>(
          factor_input_)) {
    return AuthFactorType::kRecovery;
  }
  if (std::holds_alternative<AuthFactorInput::RecoveryAuthentication>(
          factor_input_)) {
    return AuthFactorType::kRecovery;
  }
  NOTREACHED();
}

bool AuthFactorInput::UsableForCreation() const {
  if (GetType() != AuthFactorType::kRecovery) {
    return true;
  }
  if (std::holds_alternative<AuthFactorInput::RecoveryCreation>(
          factor_input_)) {
    return true;
  }
  return false;
}

bool AuthFactorInput::UsableForAuthentication() const {
  if (GetType() != AuthFactorType::kRecovery) {
    return true;
  }
  if (std::holds_alternative<AuthFactorInput::RecoveryAuthentication>(
          factor_input_)) {
    return true;
  }
  return false;
}

const AuthFactorInput::Password& AuthFactorInput::GetPasswordInput() const {
  return std::get<AuthFactorInput::Password>(factor_input_);
}

const AuthFactorInput::Pin& AuthFactorInput::GetPinInput() const {
  return std::get<AuthFactorInput::Pin>(factor_input_);
}

const AuthFactorInput::RecoveryCreation&
AuthFactorInput::GetRecoveryCreationInput() const {
  return std::get<AuthFactorInput::RecoveryCreation>(factor_input_);
}
const AuthFactorInput::RecoveryAuthentication&
AuthFactorInput::GetRecoveryAuthenticationInput() const {
  return std::get<AuthFactorInput::RecoveryAuthentication>(factor_input_);
}
const AuthFactorInput::SmartCard& AuthFactorInput::GetSmartCardInput() const {
  return std::get<AuthFactorInput::SmartCard>(factor_input_);
}

}  // namespace cryptohome
