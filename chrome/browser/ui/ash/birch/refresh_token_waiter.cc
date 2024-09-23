// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/refresh_token_waiter.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace ash {

RefreshTokenWaiter::RefreshTokenWaiter(Profile* profile)
    : identity_manager_(IdentityManagerFactory::GetForProfile(profile)) {
  CHECK(identity_manager_);
}

RefreshTokenWaiter::~RefreshTokenWaiter() = default;

void RefreshTokenWaiter::Wait(base::OnceClosure callback) {
  CHECK(callback_.is_null()) << "Multiple waits are not supported.";

  if (identity_manager_->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSignin)) {
    // Refresh tokens already loaded, proceed.
    std::move(callback).Run();
    return;
  }
  // Store the callback for later and observe the identity manager for the
  // refresh token load.
  callback_ = std::move(callback);
  identity_manager_observation_.Observe(identity_manager_);
}

void RefreshTokenWaiter::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  const CoreAccountInfo& primary_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (account_info != primary_account_info) {
    return;
  }
  identity_manager_observation_.Reset();
  // Refresh tokens are loaded, proceed.
  std::move(callback_).Run();
}

}  // namespace ash
