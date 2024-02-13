// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_GAIA_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_GAIA_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher_factory.h"

class ProfileOAuth2TokenService;
class SigninClient;

// `AccountCapabilitiesFetcherFactory` implementation that creates
// `AccountCapabilitiesFetcherGaia` instances.
// `AccountCapabilitiesFetcherGaia` is used on platforms that fetch account
// capabilities directly from the server.
class AccountCapabilitiesFetcherFactoryGaia
    : public AccountCapabilitiesFetcherFactory {
 public:
  AccountCapabilitiesFetcherFactoryGaia(
      ProfileOAuth2TokenService* token_service,
      SigninClient* signin_client);
  ~AccountCapabilitiesFetcherFactoryGaia() override;

  AccountCapabilitiesFetcherFactoryGaia(
      const AccountCapabilitiesFetcherFactoryGaia&) = delete;
  AccountCapabilitiesFetcherFactoryGaia& operator=(
      const AccountCapabilitiesFetcherFactoryGaia&) = delete;

  // AccountCapabilitiesFetcherFactory:
  std::unique_ptr<AccountCapabilitiesFetcher> CreateAccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback)
      override;
  void PrepareForFetchingAccountCapabilities() override;

 private:
  const raw_ptr<ProfileOAuth2TokenService> token_service_;
  const raw_ptr<SigninClient> signin_client_;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_GAIA_H_
