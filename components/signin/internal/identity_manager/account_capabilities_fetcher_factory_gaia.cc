// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_fetcher_factory_gaia.h"

#include <memory>

#include "components/signin/internal/identity_manager/account_capabilities_fetcher_gaia.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_client.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

AccountCapabilitiesFetcherFactoryGaia::AccountCapabilitiesFetcherFactoryGaia(
    ProfileOAuth2TokenService* token_service,
    SigninClient* signin_client)
    : token_service_(token_service), signin_client_(signin_client) {}

AccountCapabilitiesFetcherFactoryGaia::
    ~AccountCapabilitiesFetcherFactoryGaia() = default;

std::unique_ptr<AccountCapabilitiesFetcher>
AccountCapabilitiesFetcherFactoryGaia::CreateAccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback) {
  return std::make_unique<AccountCapabilitiesFetcherGaia>(
      token_service_, signin_client_->GetURLLoaderFactory(), account_info,
      fetch_priority, std::move(on_complete_callback));
}

void AccountCapabilitiesFetcherFactoryGaia::
    PrepareForFetchingAccountCapabilities() {
  // Pre-connect the HTTPS socket to the Account Capabilities server URL.
  // This means that a fetch in the near future will be able to re-use this
  // connection, which saves on the HTTPS connection establishment round-trips.
  signin_client_->GetNetworkContext()->PreconnectSockets(
      /*num_streams=*/1, GaiaUrls::GetInstance()->account_capabilities_url(),
      google_apis::GetOmitCredentialsModeForGaiaRequests(),
      net::NetworkAnonymizationKey::CreateSameSite(net::SchemefulSite(
          GaiaUrls::GetInstance()->account_capabilities_url())));
}
