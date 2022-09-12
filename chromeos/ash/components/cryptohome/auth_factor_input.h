// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_INPUT_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_INPUT_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace cryptohome {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME) AuthFactorInput {
 public:
  struct Password {
    std::string hashed_password;
  };

  struct Pin {
    std::string hashed_pin;
  };

  struct RecoveryCreation {
    std::string pub_key;
  };

  struct RecoveryAuthentication {
    std::string epoch_data;
    std::string recovery_data;
  };

  struct SmartCard {
    // (b/241259026): introdude proper enum instead of int.
    int signature_algorithm;
    std::string key_delegate_dbus_service_name;
  };

  struct Kiosk {};

  using InputVariant = absl::variant<Password,
                                     Pin,
                                     RecoveryCreation,
                                     RecoveryAuthentication,
                                     SmartCard,
                                     Kiosk>;

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
