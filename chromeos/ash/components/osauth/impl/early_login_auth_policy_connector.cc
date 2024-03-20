// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/early_login_auth_policy_connector.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/notimplemented.h"
#include "chromeos/ash/components/early_prefs/early_prefs_reader.h"
#include "chromeos/ash/components/osauth/public/auth_policy_connector.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"

namespace ash {

EarlyLoginAuthPolicyConnector::EarlyLoginAuthPolicyConnector(
    const AccountId& account_id,
    std::unique_ptr<EarlyPrefsReader> early_prefs)
    : account_id_(account_id), early_prefs_(std::move(early_prefs)) {}

EarlyLoginAuthPolicyConnector::~EarlyLoginAuthPolicyConnector() = default;

void EarlyLoginAuthPolicyConnector::SetLoginScreenAuthPolicyConnector(
    AuthPolicyConnector* connector) {
  login_screen_connector_ = connector;
}

std::optional<bool> EarlyLoginAuthPolicyConnector::GetRecoveryInitialState(
    const AccountId& account) {
  return login_screen_connector_->GetRecoveryInitialState(account);
}

std::optional<bool> EarlyLoginAuthPolicyConnector::GetRecoveryDefaultState(
    const AccountId& account) {
  NOTIMPLEMENTED();
  return false;
}

std::optional<bool> EarlyLoginAuthPolicyConnector::GetRecoveryMandatoryState(
    const AccountId& account) {
  if (early_prefs_->HasPref(ash::prefs::kRecoveryFactorBehavior) &&
      early_prefs_->IsManaged(ash::prefs::kRecoveryFactorBehavior) &&
      !early_prefs_->IsRecommended(ash::prefs::kRecoveryFactorBehavior)) {
    return early_prefs_->GetValue(ash::prefs::kRecoveryFactorBehavior)
        ->GetIfBool();
  }
  return std::nullopt;
}

bool EarlyLoginAuthPolicyConnector::IsAuthFactorManaged(
    const AccountId& account,
    AshAuthFactor auth_factor) {
  if (auth_factor == AshAuthFactor::kRecovery) {
    return early_prefs_->HasPref(ash::prefs::kRecoveryFactorBehavior);
  }
  NOTIMPLEMENTED();
  return false;
}

bool EarlyLoginAuthPolicyConnector::IsAuthFactorUserModifiable(
    const AccountId& account,
    AshAuthFactor auth_factor) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace ash
