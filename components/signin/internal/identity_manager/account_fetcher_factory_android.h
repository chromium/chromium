// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_FETCHER_FACTORY_ANDROID_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_FETCHER_FACTORY_ANDROID_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/signin/internal/identity_manager/account_fetcher_factory.h"

class AccountCapabilitiesFetcher;
class ProfileOAuth2TokenService;
class SigninClient;
struct CoreAccountId;
struct CoreAccountInfo;

// `AccountFetcherFactory` implementation that creates
// `AccountCapabilitiesFetcherAndroid` instances.
// `AccountCapabilitiesFetcherAndroid` calls a GMSCore API from Java to obtain
// capabilities values.
class AccountFetcherFactoryAndroid : public AccountFetcherFactory {
 public:
  AccountFetcherFactoryAndroid(ProfileOAuth2TokenService& token_service,
                               SigninClient& signin_client);
  ~AccountFetcherFactoryAndroid() override;

  AccountFetcherFactoryAndroid(const AccountFetcherFactoryAndroid&) = delete;
  AccountFetcherFactoryAndroid& operator=(const AccountFetcherFactoryAndroid&) =
      delete;

  // AccountFetcherFactory:
  std::unique_ptr<AccountInfoFetcher> CreateAccountInfoFetcher(
      const CoreAccountId& account_id,
      base::OnceCallback<void(std::optional<AccountInfo>)> callback) override;
  std::unique_ptr<AccountCapabilitiesFetcher> CreateAccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback)
      override;

 private:
  const raw_ref<ProfileOAuth2TokenService> token_service_;
  const raw_ref<SigninClient> signin_client_;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_FETCHER_FACTORY_ANDROID_H_
