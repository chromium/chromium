// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui_handler_impl.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_callback.pb.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_dialog.h"
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

void ParentAccessUIHandlerImpl::GetParentAccessParams(
    GetParentAccessParamsCallback callback) {
  std::move(callback).Run(
      ParentAccessDialog::GetInstance()->CloneParentAccessParams());
  return;
}

void ParentAccessUIHandlerImpl::OnParentAccessDone(
    parent_access_ui::mojom::ParentAccessResult result,
    OnParentAccessDoneCallback callback) {
  auto dialog_result = std::make_unique<ParentAccessDialog::Result>();

  switch (result) {
    case parent_access_ui::mojom::ParentAccessResult::kApproved:
      DCHECK(parent_access_token_);
      dialog_result->status = ParentAccessDialog::Result::Status::kApproved;
      dialog_result->parent_access_token = parent_access_token_->token();
      // Only keep the seconds, not the nanoseconds.
      dialog_result->parent_access_token_expire_timestamp =
          base::Time::FromDoubleT(
              parent_access_token_->expire_time().seconds());
      break;
    case parent_access_ui::mojom::ParentAccessResult::kDeclined:
      dialog_result->status = ParentAccessDialog::Result::Status::kDeclined;
      break;
    case parent_access_ui::mojom::ParentAccessResult::kCancelled:
      dialog_result->status = ParentAccessDialog::Result::Status::kCancelled;
      break;
    case parent_access_ui::mojom::ParentAccessResult::kError:
      dialog_result->status = ParentAccessDialog::Result::Status::kError;
      break;
  }

  ParentAccessDialog::GetInstance()->SetResultAndClose(
      std::move(dialog_result));
  std::move(callback).Run();
}

const kids::platform::parentaccess::client::proto::ParentAccessToken*
ParentAccessUIHandlerImpl::GetParentAccessTokenForTest() {
  return parent_access_token_.get();
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

  switch (parent_access_callback.callback_case()) {
    case kids::platform::parentaccess::client::proto::ParentAccessCallback::
        CallbackCase::kOnParentVerified:
      message->type = parent_access_ui::mojom::ParentAccessServerMessageType::
          kParentVerified;

      if (parent_access_callback.on_parent_verified()
              .verification_proof_case() ==
          kids::platform::parentaccess::client::proto::OnParentVerified::
              VerificationProofCase::kParentAccessToken) {
        DCHECK(!parent_access_token_);
        parent_access_token_ = std::make_unique<
            kids::platform::parentaccess::client::proto::ParentAccessToken>();
        parent_access_token_->CopyFrom(
            parent_access_callback.on_parent_verified().parent_access_token());
      }
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
