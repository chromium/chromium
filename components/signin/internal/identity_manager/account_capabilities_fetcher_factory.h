// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_H_

#include <memory>

#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"

class AccountCapabilitiesFetcher;
struct CoreAccountInfo;

// Abstract factory class for creating `AccountCapabilitiesFetcher` objects.
class AccountCapabilitiesFetcherFactory {
 public:
  virtual ~AccountCapabilitiesFetcherFactory() = default;

  virtual std::unique_ptr<AccountCapabilitiesFetcher>
  CreateAccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback) = 0;

  // Calling this method provides a hint that
  // `CreateAccountCapabilitiesFetcher()` may be called in the near future, and
  // front-loads some processing to speed up future fetches.
  //
  // This method can be called at any time (regardless of whether any
  // `AccountCapabilityFetcher` instances have been created and/or fetches are
  // in progress). It is OK if no subsequent fetch is triggered (and there is no
  // need to clean up state).
  //
  // The implementation depends on platform (and may be a no-op).
  virtual void PrepareForFetchingAccountCapabilities() {}
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_H_
