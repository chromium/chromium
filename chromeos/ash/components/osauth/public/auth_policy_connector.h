// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_POLICY_CONNECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_POLICY_CONNECTOR_H_

#include <optional>

#include "base/component_export.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"

namespace ash {

// Common interface for accessing authentication related policy values.
// Has different implementations which will be used depending on the user login
// stage. After the user profile is loaded, it will access profile prefs to get
// the policy value. Before profile is loaded, but after the user data directory
// is mounted, it will use the backup of the pref values saved on disk.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthPolicyConnector {
 public:
  // Convenience method.
  static inline AuthPolicyConnector* Get() {
    return AuthParts::Get()->GetAuthPolicyConnector();
  }

  virtual void SetLoginScreenAuthPolicyConnector(
      AuthPolicyConnector* connector) {}

  // Returns `true` if the recovery opt-in UIs should be shown for the user, and
  // `false` otherwise.
  virtual std::optional<bool> GetRecoveryInitialState(
      const AccountId& account) = 0;
  // Returns `true` if the recovery auth factor should be activated (by default
  // or by policy), and `false` otherwise.
  // - For non-managed users this value should be
  // used only in opt-in UIs. In-session - call cryptohome to find out whether
  // recovery factor is configured.
  // - For managed users this value may change due to
  // the policy change and may not correspond to the actual state in cryptohome.
  virtual std::optional<bool> GetRecoveryDefaultState(
      const AccountId& account) = 0;

  // Returns non-empty value if the recovery factor is enforced by the policy.
  virtual std::optional<bool> GetRecoveryMandatoryState(
      const AccountId& account) = 0;

  virtual bool IsAuthFactorManaged(const AccountId& account,
                                   AshAuthFactor auth_factor) = 0;
  virtual bool IsAuthFactorUserModifiable(const AccountId& account,
                                          AshAuthFactor auth_factor) = 0;
  virtual void OnShutdown() {}

  virtual ~AuthPolicyConnector() = default;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_POLICY_CONNECTOR_H_
