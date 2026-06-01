// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"

AccountCapabilitiesFetcher::AccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    FetchPriority fetch_priority,
    OnSomeCapabilitiesFetchedCallback on_some_capabilities_fetched_callback,
    OnAllFetchesCompleteCallback on_all_fetches_complete_callback)
    : account_info_(account_info),
      fetch_priority_(fetch_priority),
      on_some_capabilities_fetched_callback_(
          std::move(on_some_capabilities_fetched_callback)),
      on_all_fetches_complete_callback_(
          std::move(on_all_fetches_complete_callback)) {
  CHECK(on_some_capabilities_fetched_callback_);
  CHECK(on_all_fetches_complete_callback_);
}

AccountCapabilitiesFetcher::~AccountCapabilitiesFetcher() = default;

void AccountCapabilitiesFetcher::Start() {
  DCHECK(!started_);
  started_ = true;
  StartImpl();
}

void AccountCapabilitiesFetcher::UpdateAndCompleteFetchAndMaybeDestroySelf(
    const std::optional<AccountCapabilities>& capabilities) {
  if (capabilities.has_value()) {
    UpdateFetchedCapabilities(*capabilities);
  }
  CompleteFetchAndMaybeDestroySelf();
}

void AccountCapabilitiesFetcher::UpdateFetchedCapabilities(
    const AccountCapabilities& capabilities) {
  CHECK(on_some_capabilities_fetched_callback_);
  on_some_capabilities_fetched_callback_.Run(account_info_.account_id,
                                             capabilities);
}

void AccountCapabilitiesFetcher::CompleteFetchAndMaybeDestroySelf() {
  CHECK(on_all_fetches_complete_callback_);
  std::move(on_all_fetches_complete_callback_).Run(account_info_.account_id);
}
