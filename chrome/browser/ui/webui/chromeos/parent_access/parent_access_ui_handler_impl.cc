// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui_handler_impl.h"

#include <string>

#include "base/base64.h"
#include "base/notreached.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_callback.pb.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
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
  scopes.insert(GaiaConstants::kParentApprovalOAuth2Scope);
  scopes.insert(GaiaConstants::kProgrammaticChallengeOAuth2Scope);

  if (oauth2_access_token_fetcher_) {
    // Only one GetOAuthToken call can happen at a time.
    std::move(callback).Run(
        parent_access_ui::mojom::GetOAuthTokenStatus::kOnlyOneFetchAtATime, "");
    return;
  }

  oauth2_access_token_fetcher_ =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSync),
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

void ParentAccessUIHandlerImpl::OnParentAccessCallbackReceived(
    const std::string& encoded_parent_access_callback_proto,
    OnParentAccessCallbackReceivedCallback callback) {
  std::string decoded_parent_access_callback;
  parent_access_ui::mojom::ParentAccessServerMessagePtr message =
      parent_access_ui::mojom::ParentAccessServerMessage::New();
  if (!base::Base64Decode(encoded_parent_access_callback_proto,
                          &decoded_parent_access_callback)) {
    LOG(ERROR) << "ParentAccessHandler::ParentAccessResult: Error decoding "
                  "parent_access_result from base64";
    message->type =
        parent_access_ui::mojom::ParentAccessServerMessageType::kError;
    std::move(callback).Run(std::move(message));
    return;
  }

  kids::platform::parentaccess::client::proto::ParentAccessCallback
      parent_access_callback;
  if (!parent_access_callback.ParseFromString(decoded_parent_access_callback)) {
    LOG(ERROR) << "ParentAccessHandler::ParentAccessResult: Error parsing "
                  "decoded_parent_access_result to proto";

    message->type =
        parent_access_ui::mojom::ParentAccessServerMessageType::kError;
    std::move(callback).Run(std::move(message));
    return;
  }

  //  TODO(b/200587178): Communicate parsed callback to ChromeOS caller.

  switch (parent_access_callback.callback_case()) {
    case kids::platform::parentaccess::client::proto::ParentAccessCallback::
        CallbackCase::kOnParentVerified:

      message->type = parent_access_ui::mojom::ParentAccessServerMessageType::
          kParentVerified;

      std::move(callback).Run(std::move(message));
      break;
    default:
      LOG(ERROR)
          << "ParentAccessHandler::OnParentAccessCallback: Unknown type of "
             "callback received and ignored: "
          << parent_access_callback.callback_case();
      message->type =
          parent_access_ui::mojom::ParentAccessServerMessageType::kIgnore;
      std::move(callback).Run(std::move(message));
      break;
  }
}

}  // namespace chromeos
