// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_FACTORS_CONFIGURATION_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_FACTORS_CONFIGURATION_H_

#include <optional>
#include <string>

#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"

namespace ash {

// Public information about authentication keys configured for particular user
// and keys that are supported for the user.
// Unlike `SessionAuthFactors` that contains information about keys that can be
// used during ongoing authentication, this structure contains all AuthFactors
// configured for the user along with the information on which other keys
// can be added.
// This structure contains no sensitive data (`AuthInput`).
// This class works only with keys represented by AuthFactor.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
    AuthFactorsConfiguration final {
 public:
  AuthFactorsConfiguration(
      std::vector<cryptohome::AuthFactor> configured_factors,
      cryptohome::AuthFactorsSet supported_factors);

  // Empty constructor is needed so that UserContext can be created.
  AuthFactorsConfiguration();

  // Copy constructor (and operator) are needed because UserContext is copyable.
  AuthFactorsConfiguration(const AuthFactorsConfiguration&);
  AuthFactorsConfiguration(AuthFactorsConfiguration&&);

  ~AuthFactorsConfiguration();

  AuthFactorsConfiguration& operator=(const AuthFactorsConfiguration&);

  const cryptohome::AuthFactorsSet get_supported_factors() const {
    return supported_factors_;
  }

  // Checks if user has at least one AuthFactor of `type` configured.
  bool HasConfiguredFactor(cryptohome::AuthFactorType type) const;
  const cryptohome::AuthFactor* FindFactorByType(
      cryptohome::AuthFactorType type) const;

 private:
  std::vector<cryptohome::AuthFactor> configured_factors_;
  cryptohome::AuthFactorsSet supported_factors_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_FACTORS_CONFIGURATION_H_
