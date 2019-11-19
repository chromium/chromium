// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_SECURITY_TOKEN_PIN_TYPES_H_
#define CHROMEOS_CONSTANTS_SECURITY_TOKEN_PIN_TYPES_H_

// This header contains types related to the security token PIN requests.

namespace chromeos {

// Type of the information asked from the user during a security token PIN
// request.
// Must be kept in sync with
// chrome/browser/resources/chromeos/login/oobe_types.js.
enum class SecurityTokenPinCodeType {
  kPin = 0,
  kPuk = 1,
};

// Error to be displayed in the security token PIN request.
// Must be kept in sync with
// chrome/browser/resources/chromeos/login/oobe_types.js.
enum class SecurityTokenPinErrorLabel {
  kNone = 0,
  kUnknown = 1,
  kInvalidPin = 2,
  kInvalidPuk = 3,
  kMaxAttemptsExceeded = 4,
};

}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_SECURITY_TOKEN_PIN_TYPES_H_
