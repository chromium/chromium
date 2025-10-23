// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_complexity_checker.h"

namespace policy {

// static
bool LocalAuthFactorsComplexityChecker::CheckPasswordComplexity(
    std::string_view password) {
  return true;
}

// static
bool LocalAuthFactorsComplexityChecker::CheckPinComplexity(
    std::string_view pin) {
  return true;
}

}  // namespace policy
