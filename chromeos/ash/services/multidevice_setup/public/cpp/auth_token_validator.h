// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_AUTH_TOKEN_VALIDATOR_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_AUTH_TOKEN_VALIDATOR_H_

#include <string>

namespace ash {

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

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_AUTH_TOKEN_VALIDATOR_H_
