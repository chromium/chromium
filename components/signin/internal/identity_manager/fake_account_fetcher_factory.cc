// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/fake_account_fetcher_factory.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/signin/internal/identity_manager/account_info_fetcher_gaia.h"
#include "components/signin/internal/identity_manager/fake_account_capabilities_fetcher.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

FakeAccountFetcherFactory::FakeAccountFetcherFactory(
    ProfileOAuth2TokenService& token_service,
    SigninClient& signin_client)
    : token_service_(token_service), signin_client_(signin_client) {}

FakeAccountFetcherFactory::~FakeAccountFetcherFactory() = default;

std::unique_ptr<AccountInfoFetcher>
FakeAccountFetcherFactory::CreateAccountInfoFetcher(
    const CoreAccountId& account_id,
    base::OnceCallback<void(std::optional<AccountInfo>)> callback) {
  return std::make_unique<AccountInfoFetcherGaia>(
      token_service_.get(), signin_client_->GetURLLoaderFactory(), account_id,
      std::move(callback));
}

std::unique_ptr<AccountCapabilitiesFetcher>
FakeAccountFetcherFactory::CreateAccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnSomeCapabilitiesFetchedCallback
        on_some_capabilities_fetched_callback,
    AccountCapabilitiesFetcher::OnAllFetchesCompleteCallback
        on_all_fetches_complete_callback) {
  auto fetcher = std::make_unique<FakeAccountCapabilitiesFetcher>(
      account_info, fetch_priority,
      std::move(on_some_capabilities_fetched_callback),
      std::move(on_all_fetches_complete_callback),
      base::BindOnce(&FakeAccountFetcherFactory::OnFetcherDestroyed,
                     base::Unretained(this), account_info.account_id));
  DCHECK(!fetchers_.count(account_info.account_id));
  fetchers_[account_info.account_id] = fetcher.get();
  return fetcher;
}

void FakeAccountFetcherFactory::PrepareForFetchingAccountCapabilities() {
  num_prepare_calls_++;
}

void FakeAccountFetcherFactory::CompleteAccountCapabilitiesFetch(
    const CoreAccountId& account_id,
    const std::optional<AccountCapabilities> account_capabilities) {
  DCHECK(fetchers_.count(account_id));
  // `CompleteFetch` may destroy the fetcher.
  fetchers_[account_id]->CompleteFetch(account_capabilities);
}

void FakeAccountFetcherFactory::UpdateAccountCapabilities(
    const CoreAccountId& account_id,
    const AccountCapabilities& account_capabilities) {
  DCHECK(fetchers_.count(account_id));
  fetchers_[account_id]->UpdateCapabilities(account_capabilities);
}

void FakeAccountFetcherFactory::
    CompleteAccountCapabilitiesFetchWithoutCapabilities(
        const CoreAccountId& account_id) {
  DCHECK(fetchers_.count(account_id));
  fetchers_[account_id]->CompleteFetchWithoutCapabilities();
}

int FakeAccountFetcherFactory::
    GetNumCallsToPrepareForFetchingAccountCapabilities() const {
  return num_prepare_calls_;
}

void FakeAccountFetcherFactory::OnFetcherDestroyed(
    const CoreAccountId& account_id) {
  DCHECK(fetchers_.count(account_id));
  fetchers_.erase(account_id);
}
