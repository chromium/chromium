// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PROFILE_IDENTITY_PROVIDER_H_
#define COMPONENTS_INVALIDATION_IMPL_PROFILE_IDENTITY_PROVIDER_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "components/invalidation/public/identity_provider.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace invalidation {

// An identity provider implementation that's backed by
// ProfileOAuth2TokenService and SigninManager.
class ProfileIdentityProvider : public IdentityProvider,
                                public identity::IdentityManager::Observer {
 public:
  ProfileIdentityProvider(identity::IdentityManager* identity_manager);
  ~ProfileIdentityProvider() override;

  // IdentityProvider:
  std::string GetActiveAccountId() override;
  bool IsActiveAccountAvailable() override;
  std::unique_ptr<ActiveAccountAccessTokenFetcher> FetchAccessToken(
      const std::string& oauth_consumer_name,
      const identity::ScopeSet& scopes,
      ActiveAccountAccessTokenCallback callback) override;
  void InvalidateAccessToken(const identity::ScopeSet& scopes,
                             const std::string& access_token) override;
  void SetActiveAccountId(const std::string& account_id) override;

  // identity::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(const AccountInfo& account_info,
                                       bool is_valid) override;
  void OnRefreshTokenRemovedForAccount(const std::string& account_id) override;

 private:
  identity::IdentityManager* const identity_manager_;

  std::string active_account_id_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIdentityProvider);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_PROFILE_IDENTITY_PROVIDER_H_
