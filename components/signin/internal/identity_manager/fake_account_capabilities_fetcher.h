// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_ACCOUNT_CAPABILITIES_FETCHER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_ACCOUNT_CAPABILITIES_FETCHER_H_

#include "base/functional/callback_forward.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"

struct CoreAccountInfo;
class AccountCapabilities;

// Fake `AccountCapabilitiesFetcher` implementation for tests.
class FakeAccountCapabilitiesFetcher : public AccountCapabilitiesFetcher {
 public:
  explicit FakeAccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      OnCompleteCallback on_complete_callback,
      base::OnceClosure on_destroy_callback);
  ~FakeAccountCapabilitiesFetcher() override;

  FakeAccountCapabilitiesFetcher(const FakeAccountCapabilitiesFetcher&) =
      delete;
  FakeAccountCapabilitiesFetcher& operator=(
      const FakeAccountCapabilitiesFetcher&) = delete;

  void CompleteFetch(
      const std::optional<AccountCapabilities>& account_capabilities);

 protected:
  // AccountCapabilitiesFetcher:
  void StartImpl() override;

 private:
  base::OnceClosure on_destroy_callback_;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_ACCOUNT_CAPABILITIES_FETCHER_H_
