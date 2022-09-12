// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_FACTORS_DATA_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_FACTORS_DATA_H_

#include <string>

#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Public information about authentication keys configured for particular user.
// This class partially encapsulates implementation details of key definition
// (cryptohome::KeyData vs cryptohome::AuthFactor).
// Note that this information does not contain any key secrets.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
    AuthFactorsData {
 public:
  explicit AuthFactorsData(std::vector<cryptohome::KeyDefinition> keys);
  explicit AuthFactorsData(
      std::vector<cryptohome::AuthFactor> configured_factors);

  // Empty constructor is needed so that UserContext can be created.
  AuthFactorsData();
  // Copy constructor (and operator) are needed because UserContext is copyable.
  AuthFactorsData(const AuthFactorsData&);
  AuthFactorsData(AuthFactorsData&&);

  ~AuthFactorsData();

  AuthFactorsData& operator=(const AuthFactorsData&);

  // Legacy Key-based API:

  // Returns metadata for the Password key, so that it can be identified for
  // further operations.
  const cryptohome::KeyDefinition* FindOnlinePasswordKey() const;

  // Returns metadata for the Kiosk key, so that it can be identified for
  // further operations.
  const cryptohome::KeyDefinition* FindKioskKey() const;

  // Checks if password key with given label exists.
  bool HasPasswordKey(const std::string& label) const;

  // Returns metadata for the PIN key, so that it can be identified for
  // further operations.
  const cryptohome::KeyDefinition* FindPinKey() const;

  const cryptohome::AuthFactor* FindPasswordFactor(
      const cryptohome::KeyLabel& label) const;
  const cryptohome::AuthFactor* FindOnlinePasswordFactor() const;
  const cryptohome::AuthFactor* FindKioskFactor() const;
  const cryptohome::AuthFactor* FindPinFactor() const;
  const cryptohome::AuthFactor* FindRecoveryFactor() const;

 private:
  const cryptohome::AuthFactor* FindFactorByType(
      cryptohome::AuthFactorType type) const;

  std::vector<cryptohome::KeyDefinition> keys_;
  std::vector<cryptohome::AuthFactor> configured_factors_;
  cryptohome::AuthFactorsSet supported_factors_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_FACTORS_DATA_H_
