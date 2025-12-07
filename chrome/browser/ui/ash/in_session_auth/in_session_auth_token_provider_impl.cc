// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/in_session_auth/in_session_auth_token_provider_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/in_session_auth/in_session_auth_dialog_controller_impl.h"
#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"

namespace ash {

InSessionAuthTokenProviderImpl::InSessionAuthTokenProviderImpl() {
  ash::Shell::Get()->in_session_auth_dialog_controller()->SetTokenProvider(
      this);
}

void InSessionAuthTokenProviderImpl::ExchangeForToken(
    std::unique_ptr<UserContext> user_context,
    OnAuthTokenGenerated callback) {
  AuthProofToken token =
      AuthSessionStorage::Get()->Store(std::move(user_context));
  std::move(callback).Run(token, cryptohome::kAuthsessionInitialLifetime);
}

}  // namespace ash
