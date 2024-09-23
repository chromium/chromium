// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PROFILE_IDENTITY_PROVIDER_H_
#define COMPONENTS_INVALIDATION_IMPL_PROFILE_IDENTITY_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace invalidation {

// An identity provider implementation that's backed by IdentityManager
class ProfileIdentityProvider : public IdentityProvider,
                                public signin::IdentityManager::Observer {
 public:
  explicit ProfileIdentityProvider(signin::IdentityManager* identity_manager);
  ProfileIdentityProvider(const ProfileIdentityProvider& other) = delete;
  ProfileIdentityProvider& operator=(const ProfileIdentityProvider& other) =
      delete;
  ~ProfileIdentityProvider() override;

  // IdentityProvider:
  CoreAccountId GetActiveAccountId() override;
  bool IsActiveAccountWithRefreshToken() override;
  std::unique_ptr<ActiveAccountAccessTokenFetcher> FetchAccessToken(
      const std::string& oauth_consumer_name,
      const signin::ScopeSet& scopes,
      ActiveAccountAccessTokenCallback callback) override;
  void InvalidateAccessToken(const signin::ScopeSet& scopes,
                             const std::string& access_token) override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

 private:
  const raw_ptr<signin::IdentityManager> identity_manager_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_PROFILE_IDENTITY_PROVIDER_H_
