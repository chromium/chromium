// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_SESSION_AUTH_FACTORS_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_SESSION_AUTH_FACTORS_H_

#include <optional>
#include <string>

#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"

namespace ash {

// Public information about authentication keys available for authentication.
// This class partially encapsulates implementation details of key definition
// (cryptohome::KeyData vs cryptohome::AuthFactor).
// Note that this information does not contain any key secrets.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
    SessionAuthFactors final {
 public:
  explicit SessionAuthFactors(std::vector<cryptohome::KeyDefinition> keys);
  explicit SessionAuthFactors(
      std::vector<cryptohome::AuthFactor> session_factors);

  // Empty constructor is needed so that UserContext can be created.
  SessionAuthFactors();
  // Copy constructor (and operator) are needed because UserContext is copyable.
  SessionAuthFactors(const SessionAuthFactors&);
  SessionAuthFactors(SessionAuthFactors&&);

  ~SessionAuthFactors();

  SessionAuthFactors& operator=(const SessionAuthFactors&);

  // Legacy Key-based API:

  // Returns metadata for the Password key, so that it can be identified for
  // further operations.
  const cryptohome::KeyDefinition* FindOnlinePasswordKey() const;

  // Returns metadata for the Kiosk key, so that it can be identified for
  // further operations.
  const cryptohome::KeyDefinition* FindKioskKey() const;

  // Checks if password key with given label exists.
  bool HasPasswordKey(const std::string& label) const;

  bool HasSinglePasswordFactor() const;

  // Returns metadata for the PIN key, so that it can be identified for
  // further operations.
  const cryptohome::KeyDefinition* FindPinKey() const;

  const cryptohome::AuthFactor* FindPasswordFactor(
      const cryptohome::KeyLabel& label) const;
  const cryptohome::AuthFactor* FindOnlinePasswordFactor() const;
  const cryptohome::AuthFactor* FindLocalPasswordFactor() const;
  const cryptohome::AuthFactor* FindAnyPasswordFactor() const;
  const cryptohome::AuthFactor* FindKioskFactor() const;
  const cryptohome::AuthFactor* FindPinFactor() const;
  const cryptohome::AuthFactor* FindRecoveryFactor() const;
  const cryptohome::AuthFactor* FindSmartCardFactor() const;
  const std::vector<cryptohome::AuthFactorType> GetSessionFactors() const;
  const std::vector<cryptohome::KeyLabel> GetFactorLabelsByType(
      cryptohome::AuthFactorType type) const;

  const cryptohome::AuthFactor* FindFactorByType(
      cryptohome::AuthFactorType type) const;

 private:
  // Depending on the state of eatures::IsUseAuthFactorsEnabled() only
  // one of these two vectors would be filled.
  std::vector<cryptohome::KeyDefinition> keys_;
  std::vector<cryptohome::AuthFactor> session_factors_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_SESSION_AUTH_FACTORS_H_
