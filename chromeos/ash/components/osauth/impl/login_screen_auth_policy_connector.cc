// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/login_screen_auth_policy_connector.h"

#include <optional>

#include "base/notimplemented.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"

namespace ash {

namespace {
bool IsUserManaged(PrefService* local_state, const AccountId& account) {
  user_manager::KnownUser known_user(local_state);
  return known_user.GetIsEnterpriseManaged(account);
}
}  // namespace

LoginScreenAuthPolicyConnector::LoginScreenAuthPolicyConnector(
    PrefService* local_state)
    : local_state_(local_state) {}

LoginScreenAuthPolicyConnector::~LoginScreenAuthPolicyConnector() = default;

std::optional<bool> LoginScreenAuthPolicyConnector::GetRecoveryInitialState(
    const AccountId& account) {
  return !IsUserManaged(local_state_, account);
}

std::optional<bool> LoginScreenAuthPolicyConnector::GetRecoveryDefaultState(
    const AccountId& account) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<bool> LoginScreenAuthPolicyConnector::GetRecoveryMandatoryState(
    const AccountId& account) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

bool LoginScreenAuthPolicyConnector::IsAuthFactorManaged(
    const AccountId& account,
    AshAuthFactor auth_factor) {
  NOTIMPLEMENTED();
  return false;
}

bool LoginScreenAuthPolicyConnector::IsAuthFactorUserModifiable(
    const AccountId& account,
    AshAuthFactor auth_factor) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace ash
