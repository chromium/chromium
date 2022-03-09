// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/fake_account_capabilities_fetcher_factory.h"

#include "base/bind.h"
#include "components/signin/internal/identity_manager/fake_account_capabilities_fetcher.h"
#include "components/signin/public/identity_manager/account_info.h"

FakeAccountCapabilitiesFetcherFactory::FakeAccountCapabilitiesFetcherFactory() =
    default;
FakeAccountCapabilitiesFetcherFactory::
    ~FakeAccountCapabilitiesFetcherFactory() = default;

std::unique_ptr<AccountCapabilitiesFetcher>
FakeAccountCapabilitiesFetcherFactory::CreateAccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback) {
  auto fetcher = std::make_unique<FakeAccountCapabilitiesFetcher>(
      account_info,
      base::BindOnce(&FakeAccountCapabilitiesFetcherFactory::OnFetchComplete,
                     base::Unretained(this), std::move(on_complete_callback)));
  DCHECK(!fetchers_.count(account_info.account_id));
  fetchers_[account_info.account_id] = fetcher.get();
  return fetcher;
}

void FakeAccountCapabilitiesFetcherFactory::CompleteAccountCapabilitiesFetch(
    const CoreAccountId& account_id,
    const absl::optional<AccountCapabilities> account_capabilities) {
  DCHECK(fetchers_.count(account_id));
  // `CompleteFetch` may destroy the fetcher.
  fetchers_[account_id]->CompleteFetch(account_capabilities);
}

void FakeAccountCapabilitiesFetcherFactory::OnFetchComplete(
    AccountCapabilitiesFetcher::OnCompleteCallback callback,
    const CoreAccountId& account_id,
    const absl::optional<AccountCapabilities>& account_capabilities) {
  fetchers_.erase(account_id);
  std::move(callback).Run(account_id, account_capabilities);
}
