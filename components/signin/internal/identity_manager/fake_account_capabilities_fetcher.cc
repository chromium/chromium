// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/fake_account_capabilities_fetcher.h"

#include "google_apis/gaia/core_account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

FakeAccountCapabilitiesFetcher::FakeAccountCapabilitiesFetcher(
    const CoreAccountId& account_id,
    OnCompleteCallback on_complete_callback)
    : AccountCapabilitiesFetcher(account_id, std::move(on_complete_callback)) {}
FakeAccountCapabilitiesFetcher::~FakeAccountCapabilitiesFetcher() = default;

void FakeAccountCapabilitiesFetcher::Start() {}

void FakeAccountCapabilitiesFetcher::CompleteFetch(
    const absl::optional<AccountCapabilities>& account_capabilities) {
  CompleteFetchAndMaybeDestroySelf(account_capabilities);
}
