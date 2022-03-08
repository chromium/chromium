// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_ACCOUNT_CAPABILITIES_FETCHER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_ACCOUNT_CAPABILITIES_FETCHER_H_

#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"

struct CoreAccountId;
class AccountCapabilities;

// Fake `AccountCapabilitiesFetcher` implementation for tests.
class FakeAccountCapabilitiesFetcher : public AccountCapabilitiesFetcher {
 public:
  explicit FakeAccountCapabilitiesFetcher(
      const CoreAccountId& account_id,
      OnCompleteCallback on_complete_callback);
  ~FakeAccountCapabilitiesFetcher() override;

  FakeAccountCapabilitiesFetcher(const FakeAccountCapabilitiesFetcher&) =
      delete;
  FakeAccountCapabilitiesFetcher& operator=(
      const FakeAccountCapabilitiesFetcher&) = delete;

  // AccountCapabilitiesFetcher:
  void Start() override;

  void CompleteFetch(
      const absl::optional<AccountCapabilities>& account_capabilities);
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_ACCOUNT_CAPABILITIES_FETCHER_H_
