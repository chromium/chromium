// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_fetcher_factory_android.h"

#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher_android.h"
#include "components/signin/public/identity_manager/account_info.h"

AccountCapabilitiesFetcherFactoryAndroid::
    AccountCapabilitiesFetcherFactoryAndroid() = default;
AccountCapabilitiesFetcherFactoryAndroid::
    ~AccountCapabilitiesFetcherFactoryAndroid() = default;

std::unique_ptr<AccountCapabilitiesFetcher>
AccountCapabilitiesFetcherFactoryAndroid::CreateAccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback) {
  return std::make_unique<AccountCapabilitiesFetcherAndroid>(
      account_info, fetch_priority, std::move(on_complete_callback));
}
