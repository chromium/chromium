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
      AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback) = 0;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_H_
