// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_GENERATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_GENERATOR_H_

#include "base/strings/string16.h"

namespace autofill {

class PasswordRequirementsSpec;

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
base::string16 GeneratePassword(const PasswordRequirementsSpec& spec);

}  // namespace autofill

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_GENERATOR_H_
