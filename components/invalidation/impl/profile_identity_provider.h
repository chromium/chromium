// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PROFILE_IDENTITY_PROVIDER_H_
#define COMPONENTS_INVALIDATION_IMPL_PROFILE_IDENTITY_PROVIDER_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace invalidation {

// An identity provider implementation that's backed by IdentityManager
class ProfileIdentityProvider : public IdentityProvider,
                                public signin::IdentityManager::Observer {
 public:
  ProfileIdentityProvider(signin::IdentityManager* identity_manager);
  ~ProfileIdentityProvider() override;

  // IdentityProvider:
  CoreAccountId GetActiveAccountId() override;
  bool IsActiveAccountWithRefreshToken() override;
  std::unique_ptr<ActiveAccountAccessTokenFetcher> FetchAccessToken(
      const std::string& oauth_consumer_name,
      const identity::ScopeSet& scopes,
      ActiveAccountAccessTokenCallback callback) override;
  void InvalidateAccessToken(const identity::ScopeSet& scopes,
                             const std::string& access_token) override;
  void SetActiveAccountId(const CoreAccountId& account_id) override;

  // signin::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;

 private:
  signin::IdentityManager* const identity_manager_;

  CoreAccountId active_account_id_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIdentityProvider);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_PROFILE_IDENTITY_PROVIDER_H_
