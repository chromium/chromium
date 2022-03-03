// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"

AccountCapabilitiesFetcher::AccountCapabilitiesFetcher(
    const CoreAccountId& account_id,
    OnCompleteCallback on_complete_callback)
    : account_id_(account_id),
      on_complete_callback_(std::move(on_complete_callback)) {
  DCHECK(on_complete_callback_);
}

AccountCapabilitiesFetcher::~AccountCapabilitiesFetcher() = default;

void AccountCapabilitiesFetcher::CompleteFetchAndMaybeDestroySelf(
    const absl::optional<AccountCapabilities>& capabilities) {
  DCHECK(on_complete_callback_);
  std::move(on_complete_callback_).Run(account_id_, capabilities);
}
