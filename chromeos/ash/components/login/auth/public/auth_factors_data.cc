// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/auth_factors_data.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

AuthFactorsData::AuthFactorsData(std::vector<cryptohome::KeyDefinition> keys)
    : keys_(std::move(keys)) {
  // Sort the keys by label, so that in case of ties (e.g., when choosing among
  // multiple legacy keys in `FindOnlinePasswordKey()`) we're not affected by
  // random factors that affect the input ordering of `keys`.
  std::sort(keys_.begin(), keys_.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.label.value() < rhs.label.value();
  });
}

AuthFactorsData::AuthFactorsData(
    std::vector<cryptohome::AuthFactor> configured_factors)
    : configured_factors_(std::move(configured_factors)) {}

AuthFactorsData::AuthFactorsData() = default;
AuthFactorsData::AuthFactorsData(const AuthFactorsData&) = default;
AuthFactorsData::AuthFactorsData(AuthFactorsData&&) = default;
AuthFactorsData::~AuthFactorsData() = default;
AuthFactorsData& AuthFactorsData::operator=(const AuthFactorsData&) = default;

const cryptohome::KeyDefinition* AuthFactorsData::FindOnlinePasswordKey()
    const {
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.label.value() == kCryptohomeGaiaKeyLabel)
      return &key_def;
  }
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    // Check if label starts with prefix and has required type.
    if ((key_def.label.value().find(kCryptohomeGaiaKeyLegacyLabelPrefix) ==
         0) &&
        key_def.type == cryptohome::KeyDefinition::TYPE_PASSWORD)
      return &key_def;
  }
  return nullptr;
}

const cryptohome::KeyDefinition* AuthFactorsData::FindKioskKey() const {
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.type == cryptohome::KeyDefinition::TYPE_PUBLIC_MOUNT)
      return &key_def;
  }
  return nullptr;
}

bool AuthFactorsData::HasPasswordKey(const std::string& label) const {
  DCHECK_NE(label, kCryptohomePinLabel);

  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.type == cryptohome::KeyDefinition::TYPE_PASSWORD &&
        key_def.label.value() == label)
      return true;
  }
  return false;
}

const cryptohome::KeyDefinition* AuthFactorsData::FindPinKey() const {
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.type == cryptohome::KeyDefinition::TYPE_PASSWORD &&
        key_def.policy.low_entropy_credential) {
      DCHECK_EQ(key_def.label.value(), kCryptohomePinLabel);
      return &key_def;
    }
  }
  return nullptr;
}

const cryptohome::AuthFactor* AuthFactorsData::FindFactorByType(
    cryptohome::AuthFactorType type) const {
  auto result = base::ranges::find_if(
      configured_factors_, [type](auto& f) { return f.ref().type() == type; });
  if (result == configured_factors_.end())
    return nullptr;
  return &(*result);
}

const cryptohome::AuthFactor* AuthFactorsData::FindOnlinePasswordFactor()
    const {
  auto result = base::ranges::find_if(configured_factors_, [](auto& f) {
    if (f.ref().type() != cryptohome::AuthFactorType::kPassword)
      return false;
    auto label = f.ref().label().value();
    return label == kCryptohomeGaiaKeyLabel ||
           (label.find(kCryptohomeGaiaKeyLegacyLabelPrefix) == 0);
  });
  if (result == configured_factors_.end())
    return nullptr;
  return &(*result);
}

const cryptohome::AuthFactor* AuthFactorsData::FindPasswordFactor(
    const cryptohome::KeyLabel& label) const {
  DCHECK_NE(label.value(), kCryptohomePinLabel);

  auto result = base::ranges::find_if(configured_factors_, [&label](auto& f) {
    if (f.ref().type() != cryptohome::AuthFactorType::kPassword)
      return false;
    return f.ref().label() == label;
  });
  if (result == configured_factors_.end())
    return nullptr;
  return &(*result);
}

const cryptohome::AuthFactor* AuthFactorsData::FindKioskFactor() const {
  return FindFactorByType(cryptohome::AuthFactorType::kKiosk);
}

const cryptohome::AuthFactor* AuthFactorsData::FindPinFactor() const {
  return FindFactorByType(cryptohome::AuthFactorType::kPin);
}

const cryptohome::AuthFactor* AuthFactorsData::FindRecoveryFactor() const {
  return FindFactorByType(cryptohome::AuthFactorType::kRecovery);
}

}  // namespace ash
