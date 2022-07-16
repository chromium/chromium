// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_AUTH_TOKEN_VALIDATOR_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_AUTH_TOKEN_VALIDATOR_H_

#include <string>

namespace chromeos {

namespace multidevice_setup {

// Validates a given auth token.
class AuthTokenValidator {
 public:
  AuthTokenValidator() = default;

  AuthTokenValidator(const AuthTokenValidator&) = delete;
  AuthTokenValidator& operator=(const AuthTokenValidator&) = delete;

  virtual ~AuthTokenValidator() = default;

  virtual bool IsAuthTokenValid(const std::string& auth_token) = 0;
};

}  // namespace multidevice_setup

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
namespace multidevice_setup {
using ::chromeos::multidevice_setup::AuthTokenValidator;
}
}  // namespace ash

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_AUTH_TOKEN_VALIDATOR_H_
