// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_H_

#include <map>
#include <memory>
#include <optional>

#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher_factory.h"

class FakeAccountCapabilitiesFetcher;
class AccountCapabilities;
struct CoreAccountId;
struct CoreAccountInfo;

// Fake `AccountCapabilitiesFetcherFactory` implementation for tests.
class FakeAccountCapabilitiesFetcherFactory
    : public AccountCapabilitiesFetcherFactory {
 public:
  FakeAccountCapabilitiesFetcherFactory();
  ~FakeAccountCapabilitiesFetcherFactory() override;

  FakeAccountCapabilitiesFetcherFactory(
      const FakeAccountCapabilitiesFetcherFactory&) = delete;
  FakeAccountCapabilitiesFetcherFactory& operator=(
      const FakeAccountCapabilitiesFetcherFactory&) = delete;

  // AccountCapabilitiesFetcherFactory:
  std::unique_ptr<AccountCapabilitiesFetcher> CreateAccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback)
      override;
  void PrepareForFetchingAccountCapabilities() override;

  void CompleteAccountCapabilitiesFetch(
      const CoreAccountId& account_id,
      const std::optional<AccountCapabilities> account_capabilities);

  int GetNumCallsToPrepareForFetchingAccountCapabilities() const;

 private:
  void OnFetcherDestroyed(const CoreAccountId& account_id);

  std::map<CoreAccountId, FakeAccountCapabilitiesFetcher*> fetchers_;

  // The number of times `PrepareForFetchingAccountCapabilities()` has been
  // called.
  int num_prepare_calls_ = 0;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_H_
