// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_EARLY_LOGIN_AUTH_POLICY_CONNECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_EARLY_LOGIN_AUTH_POLICY_CONNECTOR_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/early_prefs/early_prefs_reader.h"
#include "chromeos/ash/components/osauth/public/auth_policy_connector.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"

namespace ash {

// Implementation of the `AuthPolicyConnector` that can be used before profile
// is loaded, but after the user data directory is mounted. It uses the backup
// of the pref values saved on disk.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
    EarlyLoginAuthPolicyConnector : public AuthPolicyConnector {
 public:
  EarlyLoginAuthPolicyConnector(const AccountId& account_id,
                                std::unique_ptr<EarlyPrefsReader> reader);
  ~EarlyLoginAuthPolicyConnector() override;

  void SetLoginScreenAuthPolicyConnector(
      AuthPolicyConnector* connector) override;

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
  AccountId account_id_;
  std::unique_ptr<EarlyPrefsReader> early_prefs_;
  raw_ptr<AuthPolicyConnector> login_screen_connector_ = nullptr;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_EARLY_LOGIN_AUTH_POLICY_CONNECTOR_H_
