// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gapis/gapis_service.h"

#include "base/command_line.h"
#include "components/gapis/gapis_manager.h"
#include "components/gapis/token_downloader.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/sync/base/sync_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace gapis {

GapisService::GapisService(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      service_url_(
          syncer::GetSyncServiceURL(*base::CommandLine::ForCurrentProcess(),
                                    channel)) {
  CHECK(base::FeatureList::IsEnabled(kEnableGapis));
}

GapisService::~GapisService() = default;

void GapisService::FetchAppTokenIfNeeded() {
  if (ongoing_access_token_fetch_ || ongoing_app_token_downloader_) {
    // Access token fetch is already in progress.
    return;
  }

  ongoing_access_token_fetch_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          signin::OAuthConsumerId::kGapisService, &identity_manager_.get(),
          base::BindOnce(&GapisService::OnAccessTokenFetched,
                         base::Unretained(this)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

void GapisService::OnAccessTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  ongoing_access_token_fetch_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    // Access token fetch failed.
    return;
  }

  CHECK(!ongoing_app_token_downloader_);

  // Use the sync service URL as the base URL for the GAPIS service.
  ongoing_app_token_downloader_ =
      std::make_unique<TokenDownloader>(service_url_, url_loader_factory_);

  // TODO(b/485217840): Fetch and sign challenge.
  std::string signed_challenge = "";
  ongoing_app_token_downloader_->FetchToken(
      base::BindOnce(&GapisService::OnAppTokenFetched, base::Unretained(this)),
      access_token_info.token, signed_challenge);
}

void GapisService::OnAppTokenFetched(const std::string& app_token) {
  ongoing_app_token_downloader_.reset();
  CHECK(!ongoing_access_token_fetch_);
  if (app_token.empty()) {
    return;
  }

  // Override the app token in the GAPIS manager.
  // TODO(b/485217840): Add token TTL to expire tokens.
  GapisManager::GetInstance()->SetAppToken(app_token);
}

}  // namespace gapis
