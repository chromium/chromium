// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_INPUT_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_INPUT_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace cryptohome {

using ::ash::ChallengeResponseKey;

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) AuthFactorInput {
 public:
  struct Password {
    std::string hashed_password;
  };

  struct Pin {
    std::string hashed_pin;
  };

  struct RecoveryCreation {
    RecoveryCreation(const std::string& pub_key,
                     const std::string& user_gaia_id,
                     const std::string& device_user_id,
                     bool ensure_fresh_recovery_id);
    RecoveryCreation(const RecoveryCreation& other);
    RecoveryCreation& operator=(const RecoveryCreation&);
    ~RecoveryCreation();
    std::string pub_key;
    std::string user_gaia_id;
    std::string device_user_id;
    bool ensure_fresh_recovery_id;
  };

  struct RecoveryAuthentication {
    std::string epoch_data;
    std::string recovery_data;
  };

  struct SmartCard {
    SmartCard(std::vector<ChallengeResponseKey::SignatureAlgorithm>
                  signature_algorithms,
              std::string key_delegate_dbus_service_name);
    SmartCard(const SmartCard& other);
    SmartCard& operator=(const SmartCard&);
    ~SmartCard();
    std::vector<ChallengeResponseKey::SignatureAlgorithm> signature_algorithms;
    std::string key_delegate_dbus_service_name;
  };

  struct Kiosk {};

  struct LegacyFingerprint {};

  struct Fingerprint {};

  using InputVariant = absl::variant<Password,
                                     Pin,
                                     RecoveryCreation,
                                     RecoveryAuthentication,
                                     SmartCard,
                                     Kiosk,
                                     LegacyFingerprint,
                                     Fingerprint>;

  explicit AuthFactorInput(InputVariant input);

  AuthFactorInput(AuthFactorInput&&) noexcept;
  AuthFactorInput& operator=(AuthFactorInput&&) noexcept;

  // AuthFactorInput should not be copied.
  AuthFactorInput(const AuthFactorInput&) = delete;
  AuthFactorInput& operator=(const AuthFactorInput&) = delete;
  ~AuthFactorInput();

  AuthFactorType GetType() const;
  bool UsableForCreation() const;
  bool UsableForAuthentication() const;

  // Fails if type does not match:
  const Password& GetPasswordInput() const;
  const Pin& GetPinInput() const;
  const RecoveryCreation& GetRecoveryCreationInput() const;
  const RecoveryAuthentication& GetRecoveryAuthenticationInput() const;
  const SmartCard& GetSmartCardInput() const;

 private:
  InputVariant factor_input_;
};

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_INPUT_H_
