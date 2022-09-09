// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/in_session_auth_token_provider_impl.h"

#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace ash {

InSessionAuthTokenProviderImpl::InSessionAuthTokenProviderImpl() {
  ash::Shell::Get()->in_session_auth_dialog_controller()->SetTokenProvider(
      this);
}

void InSessionAuthTokenProviderImpl::ExchangeForToken(
    std::unique_ptr<UserContext> user_context,
    OnAuthTokenGenerated callback) {
  auto* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForAccountId(
          user_context->GetAccountId());
  quick_unlock_storage->MarkStrongAuth();
  quick_unlock_storage->CreateAuthToken(*user_context);
  auto token = quick_unlock_storage->GetAuthToken()->GetUnguessableToken();
  DCHECK(token.has_value());
  std::move(callback).Run(token.value(),
                          quick_unlock::AuthToken::kTokenExpiration);
}

}  // namespace ash
