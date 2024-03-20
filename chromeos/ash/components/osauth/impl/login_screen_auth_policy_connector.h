// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_LOGIN_SCREEN_AUTH_POLICY_CONNECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_LOGIN_SCREEN_AUTH_POLICY_CONNECTOR_H_

#include <optional>
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/osauth/public/auth_policy_connector.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"

namespace ash {

// Implementation of the `AuthPolicyConnector` that can be used before profile
// is loaded, but after the user data directory is mounted. It uses the backup
// of the pref values saved on disk.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
    LoginScreenAuthPolicyConnector : public AuthPolicyConnector {
 public:
  explicit LoginScreenAuthPolicyConnector(PrefService* local_state);
  ~LoginScreenAuthPolicyConnector() override;

  std::optional<bool> GetRecoveryInitialState(
      const AccountId& account) override;
  std::optional<bool> GetRecoveryDefaultState(
      const AccountId& account) override;
  std::optional<bool> GetRecoveryMandatoryState(
      const AccountId& account) override;

  bool IsAuthFactorManaged(const AccountId& account,
                           AshAuthFactor auth_factor) override;
  bool IsAuthFactorUserModifiable(const AccountId& account,
                                  AshAuthFactor auth_factor) override;

 private:
  raw_ptr<PrefService> local_state_ = nullptr;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_LOGIN_SCREEN_AUTH_POLICY_CONNECTOR_H_
