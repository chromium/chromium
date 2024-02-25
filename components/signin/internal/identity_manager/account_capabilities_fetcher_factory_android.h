// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_ANDROID_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_ANDROID_H_

#include "components/signin/internal/identity_manager/account_capabilities_fetcher_factory.h"

#include <memory>

class AccountCapabilitiesFetcher;
struct CoreAccountInfo;

// `AccountCapabilitiesFetcherFactory` implementation that creates
// `AccountCapabilitiesFetcherAndroid` instances.
// `AccountCapabilitiesFetcherAndroid` calls a GMSCore API from Java to obtain
// capabilities values.
class AccountCapabilitiesFetcherFactoryAndroid
    : public AccountCapabilitiesFetcherFactory {
 public:
  AccountCapabilitiesFetcherFactoryAndroid();
  ~AccountCapabilitiesFetcherFactoryAndroid() override;

  AccountCapabilitiesFetcherFactoryAndroid(
      const AccountCapabilitiesFetcherFactoryAndroid&) = delete;
  AccountCapabilitiesFetcherFactoryAndroid& operator=(
      const AccountCapabilitiesFetcherFactoryAndroid&) = delete;

  // AccountCapabilitiesFetcherFactory:
  std::unique_ptr<AccountCapabilitiesFetcher> CreateAccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback)
      override;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_FACTORY_ANDROID_H_
