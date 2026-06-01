// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_fetcher_factory_android.h"

#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher_android.h"
#include "components/signin/internal/identity_manager/account_info_fetcher_gaia.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

AccountFetcherFactoryAndroid::AccountFetcherFactoryAndroid(
    ProfileOAuth2TokenService& token_service,
    SigninClient& signin_client)
    : token_service_(token_service), signin_client_(signin_client) {}

AccountFetcherFactoryAndroid::~AccountFetcherFactoryAndroid() = default;

std::unique_ptr<AccountInfoFetcher>
AccountFetcherFactoryAndroid::CreateAccountInfoFetcher(
    const CoreAccountId& account_id,
    base::OnceCallback<void(std::optional<AccountInfo>)> callback) {
  return std::make_unique<AccountInfoFetcherGaia>(
      token_service_.get(), signin_client_->GetURLLoaderFactory(), account_id,
      std::move(callback));
}

std::unique_ptr<AccountCapabilitiesFetcher>
AccountFetcherFactoryAndroid::CreateAccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnSomeCapabilitiesFetchedCallback
        on_some_capabilities_fetched_callback,
    AccountCapabilitiesFetcher::OnAllFetchesCompleteCallback
        on_all_fetches_complete_callback) {
  return std::make_unique<AccountCapabilitiesFetcherAndroid>(
      account_info, fetch_priority,
      std::move(on_some_capabilities_fetched_callback),
      std::move(on_all_fetches_complete_callback));
}
