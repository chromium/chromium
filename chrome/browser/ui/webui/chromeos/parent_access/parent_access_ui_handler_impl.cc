// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui_handler_impl.h"

#include <string>

#include "base/notreached.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/gaia_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

ParentAccessUIHandlerImpl::ParentAccessUIHandlerImpl(
    mojo::PendingReceiver<parent_access_ui::mojom::ParentAccessUIHandler>
        receiver,
    content::WebUI* web_ui,
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager),
      receiver_(this, std::move(receiver)) {}

ParentAccessUIHandlerImpl::~ParentAccessUIHandlerImpl() = default;

void ParentAccessUIHandlerImpl::GetOAuthToken(GetOAuthTokenCallback callback) {
  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kKidsSupervisionSetupChildOAuth2Scope);

  if (oauth2_access_token_fetcher_) {
    // Only one GetOAuthToken call can happen at a time.
    std::move(callback).Run(
        parent_access_ui::mojom::GetOAuthTokenStatus::kOnlyOneFetchAtATime, "");
    return;
  }

  oauth2_access_token_fetcher_ =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          "parent_access", scopes,
          base::BindOnce(&ParentAccessUIHandlerImpl::OnAccessTokenFetchComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          signin::AccessTokenFetcher::Mode::kImmediate);
}

void ParentAccessUIHandlerImpl::OnAccessTokenFetchComplete(
    GetOAuthTokenCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  oauth2_access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    DLOG(ERROR) << "ParentAccessUIHandlerImpl: OAuth2 token request failed. "
                << error.state() << ": " << error.ToString();

    std::move(callback).Run(
        parent_access_ui::mojom::GetOAuthTokenStatus::kError,
        "" /* No token */);
    return;
  }
  std::move(callback).Run(
      parent_access_ui::mojom::GetOAuthTokenStatus::kSuccess,
      access_token_info.token);
}

}  // namespace chromeos
