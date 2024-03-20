// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/osauth/impl/engines/cryptohome_smart_card_engine.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/notimplemented.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/impl/engines/cryptohome_based_engine.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/public/cryptohome_core.h"

namespace ash {

CryptohomeSmartCardEngine::CryptohomeSmartCardEngine(CryptohomeCore& core)
    : CryptohomeBasedEngine(core, AshAuthFactor::kSmartCard) {}

CryptohomeSmartCardEngine::~CryptohomeSmartCardEngine() = default;

std::optional<cryptohome::AuthFactorRef>
CryptohomeSmartCardEngine::LookUpFactor(UserContext& context) {
  const cryptohome::AuthFactor* smart_card_factor =
      context.GetAuthFactorsData().FindSmartCardFactor();
  if (!smart_card_factor) {
    return std::nullopt;
  }
  return smart_card_factor->ref();
}

void CryptohomeSmartCardEngine::OnAuthFactorUpdate(
    cryptohome::AuthFactorRef factor) {
  // TODO(b/271261365): Check this for correctness.
  NOTIMPLEMENTED();
}

bool CryptohomeSmartCardEngine::IsDisabledByPolicy() {
  // TODO(b/271261365): Check this for correctness.
  return false;
}

bool CryptohomeSmartCardEngine::IsLockedOut() {
  // TODO(b/271261365): Check this for correctness.
  return false;
}

bool CryptohomeSmartCardEngine::IsFactorSpecificRestricted() {
  // TODO(b/271261365): Check this for correctness.
  return false;
}

void CryptohomeSmartCardEngine::OnAuthAttempt(
    std::unique_ptr<UserContext> context,
    std::optional<AuthenticationError> error) {
  // TODO(b/271261365): Check this for correctness.
  NOTIMPLEMENTED();
}

CryptohomeSmartCardEngineFactory::CryptohomeSmartCardEngineFactory() = default;
CryptohomeSmartCardEngineFactory::~CryptohomeSmartCardEngineFactory() = default;

AshAuthFactor CryptohomeSmartCardEngineFactory::GetFactor() {
  return AshAuthFactor::kSmartCard;
}

std::unique_ptr<AuthFactorEngine>
CryptohomeSmartCardEngineFactory::CreateEngine(AuthHubMode mode) {
  CHECK(CryptohomeCore::Get());
  return std::make_unique<CryptohomeSmartCardEngine>(*CryptohomeCore::Get());
}

}  // namespace ash
