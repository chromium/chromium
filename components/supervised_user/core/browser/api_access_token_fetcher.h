// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_API_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_API_ACCESS_TOKEN_FETCHER_H_

#include <memory>

#include "base/memory/singleton.h"
#include "base/types/expected.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace supervised_user {

// Responsible for initialising the access token workflow. Executes the
// consuming callback when the fetch is done, and then becomes disposable.
class ApiAccessTokenFetcher {
 public:
  // For convenience, the interface of signin::PrimaryAccountAccessTokenFetcher
  // is wrapped into one value, so the decision how to handle errors is up to
  // consumers of access token fetcher.
  using Consumer = base::OnceCallback<void(
      base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>)>;
  // Non copyable.
  ApiAccessTokenFetcher() = delete;
  explicit ApiAccessTokenFetcher(signin::IdentityManager& identity_manager,
                                 const FetcherConfig& fetcher_config,
                                 Consumer consumer);
  ApiAccessTokenFetcher(const ApiAccessTokenFetcher&) = delete;
  ApiAccessTokenFetcher& operator=(const ApiAccessTokenFetcher&) = delete;
  ~ApiAccessTokenFetcher();

 private:
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);
  Consumer consumer_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      primary_account_access_token_fetcher_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_API_ACCESS_TOKEN_FETCHER_H_
