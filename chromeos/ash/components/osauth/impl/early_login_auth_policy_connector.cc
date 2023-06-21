// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/early_login_auth_policy_connector.h"

#include "base/notreached.h"

namespace ash {

EarlyLoginAuthPolicyConnector::EarlyLoginAuthPolicyConnector() = default;

EarlyLoginAuthPolicyConnector::~EarlyLoginAuthPolicyConnector() = default;

void EarlyLoginAuthPolicyConnector::SetLoginScreenAuthPolicyConnector(
    AuthPolicyConnector* connector) {
  login_screen_connector_ = connector;
}

absl::optional<bool> EarlyLoginAuthPolicyConnector::GetRecoveryInitialState(
    const AccountId& account) {
  return login_screen_connector_->GetRecoveryInitialState(account);
}

absl::optional<bool> EarlyLoginAuthPolicyConnector::GetRecoveryDefaultState(
    const AccountId& account) {
  NOTIMPLEMENTED();
  return false;
}

bool EarlyLoginAuthPolicyConnector::IsAuthFactorManaged(
    const AccountId& account,
    AshAuthFactor auth_factor) {
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
