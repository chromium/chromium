// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"

AccountCapabilitiesFetcher::AccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    FetchPriority fetch_priority,
    OnCompleteCallback on_complete_callback)
    : account_info_(account_info),
      fetch_priority_(fetch_priority),
      on_complete_callback_(std::move(on_complete_callback)) {
  DCHECK(on_complete_callback_);
}

AccountCapabilitiesFetcher::~AccountCapabilitiesFetcher() = default;

void AccountCapabilitiesFetcher::Start() {
  DCHECK(!started_);
  started_ = true;
  StartImpl();
}

void AccountCapabilitiesFetcher::CompleteFetchAndMaybeDestroySelf(
    const std::optional<AccountCapabilities>& capabilities) {
  DCHECK(on_complete_callback_);
  std::move(on_complete_callback_).Run(account_info_.account_id, capabilities);
}
