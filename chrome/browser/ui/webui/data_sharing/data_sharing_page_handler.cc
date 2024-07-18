// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/data_sharing/data_sharing_page_handler.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_constants.h"

DataSharingPageHandler::DataSharingPageHandler(
    TopChromeWebUIController* webui_controller,
    mojo::PendingReceiver<data_sharing::mojom::PageHandler> receiver,
    mojo::PendingRemote<data_sharing::mojom::Page> page)
    : webui_controller_(webui_controller),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // TODO(b/353754937): Refresh access token when it's expired.
  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_id, /*oauth_consumer_name=*/"data_sharing", /*scopes=*/
      {GaiaConstants::kPeopleApiReadWriteOAuth2Scope,
       GaiaConstants::kClearCutOAuth2Scope},
      base::BindOnce(&DataSharingPageHandler::OnAccessTokenFetched,
                     base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kImmediate);
#else
  // For non-branded build return an empty access token to bypass the
  // authentication flow.
  OnAccessTokenFetched(GoogleServiceAuthError(GoogleServiceAuthError::NONE),
                       signin::AccessTokenInfo("", base::Time::Now(), ""));
#endif
}

DataSharingPageHandler::~DataSharingPageHandler() {}

void DataSharingPageHandler::ShowUI() {
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}

Profile* DataSharingPageHandler::GetProfile() {
  CHECK(webui_controller_);
  return Profile::FromWebUI(webui_controller_->web_ui());
}

void DataSharingPageHandler::OnAccessTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  // It is safe to reset the token fetcher now.
  access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Access token auth error: state=" << error.state();
  }
  // Note: We do not do anything special for empty tokens.
  page_->OnAccessTokenFetched(access_token_info.token);
}
