// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_GENERATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_GENERATOR_H_

#include <stdint.h>

#include <string>

namespace autofill {

class PasswordRequirementsSpec;

// If neither lower nor upper case letters are included in the password
// generation alphabet, add the '0' and '1' digit to the numeric alphabet
// to increase the entropy of numeric-only passwords.
void ConditionallyAddNumericDigitsToAlphabet(PasswordRequirementsSpec* spec);

extern const uint32_t kDefaultPasswordLength;

// Returns a password that follows the |spec| as well as possible. If this is
// impossible, a password that nearly meets the requirements can be returned.
// In this case the user is asked to fix the password themselves.
//
// If |spec| is empty, a password of length |kDefaultPasswordLength| is
// generated that contains
// - at least 1 lower case latin character
// - at least 1 upper case latin character
// - at least 1 number (digit)
// - no symbols
std::u16string GeneratePassword(const PasswordRequirementsSpec& spec);

}  // namespace autofill

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_GENERATOR_H_
