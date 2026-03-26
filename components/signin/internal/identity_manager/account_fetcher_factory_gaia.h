// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_FETCHER_FACTORY_GAIA_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_FETCHER_FACTORY_GAIA_H_

#include "base/memory/raw_ref.h"
#include "components/signin/internal/identity_manager/account_fetcher_factory.h"

class ProfileOAuth2TokenService;
class SigninClient;

// `AccountFetcherFactory` implementation that creates
// `AccountCapabilitiesFetcherGaia` instances.
// `AccountCapabilitiesFetcherGaia` is used on platforms that fetch account
// capabilities directly from the server.
class AccountFetcherFactoryGaia : public AccountFetcherFactory {
 public:
  AccountFetcherFactoryGaia(ProfileOAuth2TokenService& token_service,
                            SigninClient& signin_client);
  ~AccountFetcherFactoryGaia() override;

  AccountFetcherFactoryGaia(const AccountFetcherFactoryGaia&) = delete;
  AccountFetcherFactoryGaia& operator=(const AccountFetcherFactoryGaia&) =
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
  void PrepareForFetchingAccountCapabilities() override;

 private:
  const raw_ref<ProfileOAuth2TokenService> token_service_;
  const raw_ref<SigninClient> signin_client_;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_FETCHER_FACTORY_GAIA_H_
