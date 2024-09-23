// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/auth_factor_presence_cache.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {
std::string GetAuthPurposeKey(AuthPurpose purpose) {
  return base::NumberToString(static_cast<int>(purpose));
}

std::optional<AuthFactorsSet> GetForPurpose(const base::Value::Dict& cache,
                                            AuthPurpose purpose) {
  std::string purpose_key = GetAuthPurposeKey(purpose);
  const base::Value::List* factor_list = cache.FindList(purpose_key);
  if (!factor_list) {
    return std::nullopt;
  }
  AuthFactorsSet result;
  for (const base::Value& element : *factor_list) {
    if (!element.is_int()) {
      continue;
    }
    result.Put(static_cast<AshAuthFactor>(element.GetInt()));
  }
  return result;
}

AuthFactorsSet GetWithFallback(const base::Value::Dict& cache,
                               AuthPurpose purpose) {
  {
    std::optional<AuthFactorsSet> result = GetForPurpose(cache, purpose);
    if (result.has_value()) {
      return *result;
    }
  }
  // Fallback logic:
  switch (purpose) {
    case AuthPurpose::kLogin:
      // Just assume that users have GAIA password.
      return AuthFactorsSet({AshAuthFactor::kGaiaPassword});
    case AuthPurpose::kAuthSettings:
      // Use same factors as login.
      return GetWithFallback(cache, AuthPurpose::kLogin);
    case AuthPurpose::kScreenUnlock:
      // Use same factors as login.
      return GetWithFallback(cache, AuthPurpose::kLogin);
    case AuthPurpose::kUserVerification:
      // Use same factors as lock screen.
      return GetWithFallback(cache, AuthPurpose::kScreenUnlock);
    case AuthPurpose::kWebAuthN:
      // Use same factors as screen unlock, but exclude non-cryptohome factors.
      {
        AuthFactorsSet result =
            GetWithFallback(cache, AuthPurpose::kScreenUnlock);
        result.Remove(AshAuthFactor::kLegacyPin);
        result.Remove(AshAuthFactor::kSmartUnlock);
        return result;
      }
  }
}

}  // namespace

AuthFactorPresenceCache::AuthFactorPresenceCache(PrefService* local_state) {
  known_user_ = std::make_unique<user_manager::KnownUser>(local_state);
}

AuthFactorPresenceCache::~AuthFactorPresenceCache() = default;

void AuthFactorPresenceCache::StoreFactorPresenceCache(AuthAttemptVector vector,
                                                       AuthFactorsSet factors) {
  if (user_manager::UserManager::Get()->IsEphemeralAccountId(vector.account)) {
    if (factors.empty()) {
      return;
    }
  }
  base::Value::Dict cache = known_user_->GetAuthFactorCache(vector.account);
  base::Value::List factor_list;
  for (AshAuthFactor f : factors) {
    factor_list.Append(static_cast<int>(f));
  }
  cache.Set(GetAuthPurposeKey(vector.purpose), std::move(factor_list));
  known_user_->SetAuthFactorCache(vector.account, std::move(cache));
}

AuthFactorsSet AuthFactorPresenceCache::GetExpectedFactorsPresence(
    AuthAttemptVector vector) {
  if (user_manager::UserManager::Get()->IsEphemeralAccountId(vector.account)) {
    // Assume that ephemeral users do not have any auth factors associated:
    return AuthFactorsSet();
  }
  base::Value::Dict cache = known_user_->GetAuthFactorCache(vector.account);
  return GetWithFallback(cache, vector.purpose);
}

}  // namespace ash
