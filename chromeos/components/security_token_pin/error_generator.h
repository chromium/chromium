// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SECURITY_TOKEN_PIN_ERROR_GENERATOR_H_
#define CHROMEOS_COMPONENTS_SECURITY_TOKEN_PIN_ERROR_GENERATOR_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/components/security_token_pin/constants.h"

namespace chromeos {
namespace security_token_pin {

// Generate an error message for a security pin token dialog, based on dialog
// parameters |error_label|, |attempts_left|, and |accept_input|.
COMPONENT_EXPORT(SECURITY_TOKEN_PIN)
std::u16string GenerateErrorMessage(ErrorLabel error_label,
                                    int attempts_left,
                                    bool accept_input);

}  // namespace security_token_pin
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SECURITY_TOKEN_PIN_ERROR_GENERATOR_H_
