// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"

#include <algorithm>
#include <optional>

#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"

namespace ash {

AuthFactorsConfiguration::AuthFactorsConfiguration(
    std::vector<cryptohome::AuthFactor> configured_factors,
    cryptohome::AuthFactorsSet supported_factors)
    : configured_factors_(std::move(configured_factors)),
      supported_factors_(supported_factors) {
  // Sort the keys by label, so that in case of ties (e.g., when choosing among
  // multiple legacy keys) we're not affected by implementation details that
  // affect the ordering of `configured_factors`.
  std::sort(configured_factors_.begin(), configured_factors_.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.ref().label().value() < rhs.ref().label().value();
            });
}

AuthFactorsConfiguration::AuthFactorsConfiguration() = default;
AuthFactorsConfiguration::AuthFactorsConfiguration(
    const AuthFactorsConfiguration&) = default;
AuthFactorsConfiguration::AuthFactorsConfiguration(AuthFactorsConfiguration&&) =
    default;
AuthFactorsConfiguration::~AuthFactorsConfiguration() = default;
AuthFactorsConfiguration& AuthFactorsConfiguration::operator=(
    const AuthFactorsConfiguration&) = default;

bool AuthFactorsConfiguration::HasConfiguredFactor(
    cryptohome::AuthFactorType type) const {
  return FindFactorByType(type) != nullptr;
}

const cryptohome::AuthFactor* AuthFactorsConfiguration::FindFactorByType(
    cryptohome::AuthFactorType type) const {
  const auto& result = base::ranges::find_if(
      configured_factors_, [type](auto& f) { return f.ref().type() == type; });
  if (result == configured_factors_.end())
    return nullptr;
  return &(*result);
}

}  // namespace ash
