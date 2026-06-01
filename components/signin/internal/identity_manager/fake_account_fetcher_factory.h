// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_ACCOUNT_FETCHER_FACTORY_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_ACCOUNT_FETCHER_FACTORY_H_

#include <map>
#include <memory>
#include <optional>

#include "base/memory/raw_ref.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#include "components/signin/internal/identity_manager/account_fetcher_factory.h"

class FakeAccountCapabilitiesFetcher;
class AccountCapabilities;
class ProfileOAuth2TokenService;
struct CoreAccountId;
struct CoreAccountInfo;
class SigninClient;

// Fake `AccountFetcherFactory` implementation for tests.
class FakeAccountFetcherFactory : public AccountFetcherFactory {
 public:
  FakeAccountFetcherFactory(ProfileOAuth2TokenService& token_service,
                            SigninClient& signin_client);
  ~FakeAccountFetcherFactory() override;

  FakeAccountFetcherFactory(const FakeAccountFetcherFactory&) = delete;
  FakeAccountFetcherFactory& operator=(const FakeAccountFetcherFactory&) =
      delete;

  // AccountFetcherFactory:
  std::unique_ptr<AccountInfoFetcher> CreateAccountInfoFetcher(
      const CoreAccountId& account_id,
      base::OnceCallback<void(std::optional<AccountInfo>)> callback) override;
  std::unique_ptr<AccountCapabilitiesFetcher> CreateAccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      AccountCapabilitiesFetcher::OnSomeCapabilitiesFetchedCallback
          on_some_capabilities_fetched_callback,
      AccountCapabilitiesFetcher::OnAllFetchesCompleteCallback
          on_all_fetches_complete_callback) override;
  void PrepareForFetchingAccountCapabilities() override;

  void CompleteAccountCapabilitiesFetch(
      const CoreAccountId& account_id,
      const std::optional<AccountCapabilities> account_capabilities);
  void UpdateAccountCapabilities(
      const CoreAccountId& account_id,
      const AccountCapabilities& account_capabilities);
  void CompleteAccountCapabilitiesFetchWithoutCapabilities(
      const CoreAccountId& account_id);

  int GetNumCallsToPrepareForFetchingAccountCapabilities() const;

 private:
  void OnFetcherDestroyed(const CoreAccountId& account_id);

  const raw_ref<ProfileOAuth2TokenService> token_service_;
  const raw_ref<SigninClient> signin_client_;

  std::map<CoreAccountId, FakeAccountCapabilitiesFetcher*> fetchers_;

  // The number of times `PrepareForFetchingAccountCapabilities()` has been
  // called.
  int num_prepare_calls_ = 0;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_FAKE_ACCOUNT_FETCHER_FACTORY_H_
