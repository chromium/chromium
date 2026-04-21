// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_FACTOR_CONFIGURATION_HELPER_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_FACTOR_CONFIGURATION_HELPER_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"

namespace ash {

class AuthFactorEditor;
class UserContext;

// Helper class for fetching the auth factor configuration.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH)
    AuthFactorConfigurationHelper {
 public:
  AuthFactorConfigurationHelper();
  AuthFactorConfigurationHelper(const AuthFactorConfigurationHelper&) = delete;
  AuthFactorConfigurationHelper& operator=(
      const AuthFactorConfigurationHelper&) = delete;
  ~AuthFactorConfigurationHelper();

  using CheckHasAuthFactorsCallback = base::OnceCallback<void(AuthFactorsSet)>;

  // Fetches the configured auth factors for the given `account_id` and
  // calls `callback` with the set of configured factors.
  void CheckHasAuthFactors(const AccountId& account_id,
                           CheckHasAuthFactorsCallback callback);

  // Checks if the given `account_id` has an online password configured.
  // Calls `on_has_online_password` if it does, and `on_no_online_password`
  // otherwise.
  void CheckHasOnlinePasswordAndContinue(
      const AccountId& account_id,
      base::OnceClosure on_has_online_password,
      base::OnceClosure on_no_online_password);

 private:
  // Fetches the auth factors configuration for the given `context`.
  void GetAuthFactorsConfiguration(std::unique_ptr<UserContext> context,
                                   AuthOperationCallback callback);

  void OnGetAuthFactorsConfiguration(CheckHasAuthFactorsCallback callback,
                                     std::unique_ptr<UserContext> context,
                                     std::optional<AuthenticationError> error);

  std::unique_ptr<AuthFactorEditor> editor_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_FACTOR_CONFIGURATION_HELPER_H_
