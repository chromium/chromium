// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_fetcher_factory_gaia.h"

#include <memory>

#include "components/signin/internal/identity_manager/account_capabilities_fetcher_gaia.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

AccountCapabilitiesFetcherFactoryGaia::AccountCapabilitiesFetcherFactoryGaia(
    ProfileOAuth2TokenService* token_service,
    SigninClient* signin_client)
    : token_service_(token_service), signin_client_(signin_client) {}

AccountCapabilitiesFetcherFactoryGaia::
    ~AccountCapabilitiesFetcherFactoryGaia() = default;

std::unique_ptr<AccountCapabilitiesFetcher>
AccountCapabilitiesFetcherFactoryGaia::CreateAccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback) {
  return std::make_unique<AccountCapabilitiesFetcherGaia>(
      token_service_, signin_client_->GetURLLoaderFactory(), account_info,
      std::move(on_complete_callback));
}
