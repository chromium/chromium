// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/fake_account_capabilities_fetcher_factory.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/signin/internal/identity_manager/fake_account_capabilities_fetcher.h"
#include "components/signin/public/identity_manager/account_info.h"

FakeAccountCapabilitiesFetcherFactory::FakeAccountCapabilitiesFetcherFactory() =
    default;
FakeAccountCapabilitiesFetcherFactory::
    ~FakeAccountCapabilitiesFetcherFactory() = default;

std::unique_ptr<AccountCapabilitiesFetcher>
FakeAccountCapabilitiesFetcherFactory::CreateAccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback) {
  auto fetcher = std::make_unique<FakeAccountCapabilitiesFetcher>(
      account_info, fetch_priority, std::move(on_complete_callback),
      base::BindOnce(&FakeAccountCapabilitiesFetcherFactory::OnFetcherDestroyed,
                     base::Unretained(this), account_info.account_id));
  DCHECK(!fetchers_.count(account_info.account_id));
  fetchers_[account_info.account_id] = fetcher.get();
  return fetcher;
}

void FakeAccountCapabilitiesFetcherFactory::
    PrepareForFetchingAccountCapabilities() {
  num_prepare_calls_++;
}

void FakeAccountCapabilitiesFetcherFactory::CompleteAccountCapabilitiesFetch(
    const CoreAccountId& account_id,
    const std::optional<AccountCapabilities> account_capabilities) {
  DCHECK(fetchers_.count(account_id));
  // `CompleteFetch` may destroy the fetcher.
  fetchers_[account_id]->CompleteFetch(account_capabilities);
}

int FakeAccountCapabilitiesFetcherFactory::
    GetNumCallsToPrepareForFetchingAccountCapabilities() const {
  return num_prepare_calls_;
}

void FakeAccountCapabilitiesFetcherFactory::OnFetcherDestroyed(
    const CoreAccountId& account_id) {
  DCHECK(fetchers_.count(account_id));
  fetchers_.erase(account_id);
}
