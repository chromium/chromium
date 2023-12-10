// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/recipients_fetcher_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/sharing/password_sharing_recipients_downloader.h"
#include "components/password_manager/core/browser/sharing/recipients_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using sync_pb::PasswordSharingRecipientsResponse;

namespace password_manager {
namespace {

bool HasServerRequestCompletedWithSuccess(
    const PasswordSharingRecipientsDownloader& request) {
  return request.GetAuthError().state() == GoogleServiceAuthError::NONE &&
         request.GetHttpError() == net::HTTP_OK &&
         request.GetNetError() == net::OK;
}

RecipientInfo ToRecipientInfo(const sync_pb::UserInfo& user_info) {
  RecipientInfo recipient_info;
  recipient_info.user_id = user_info.user_id();
  recipient_info.user_name = user_info.user_display_info().display_name();
  recipient_info.email = user_info.user_display_info().email();
  recipient_info.profile_image_url =
      user_info.user_display_info().profile_image_url();
  recipient_info.public_key =
      PublicKey::FromProto(user_info.cross_user_sharing_public_key());
  return recipient_info;
}

}  // namespace

RecipientsFetcherImpl::RecipientsFetcherImpl(
    version_info::Channel channel,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    raw_ptr<signin::IdentityManager> identity_manager)
    : channel_(channel),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {}

RecipientsFetcherImpl::~RecipientsFetcherImpl() = default;

void RecipientsFetcherImpl::FetchFamilyMembers(
    FetchFamilyMembersCallback callback) {
  // There can be only one request in flight at any point in time.
  if (pending_request_) {
    std::move(callback).Run(std::vector<RecipientInfo>(),
                            FetchFamilyMembersRequestStatus::kPendingRequest);
    return;
  }

  callback_ = std::move(callback);

  pending_request_ = std::make_unique<PasswordSharingRecipientsDownloader>(
      channel_, url_loader_factory_, identity_manager_.get());
  pending_request_->Start(base::BindOnce(
      &RecipientsFetcherImpl::ServerRequestCallback, base::Unretained(this)));
}

void RecipientsFetcherImpl::ServerRequestCallback() {
  if (!HasServerRequestCompletedWithSuccess(*pending_request_)) {
    // Destroy the request object after the response was fetched otherwise no
    // further call can be made.
    pending_request_.reset();
    std::move(callback_).Run(std::vector<RecipientInfo>(),
                             FetchFamilyMembersRequestStatus::kNetworkError);
    return;
  }

  std::optional<sync_pb::PasswordSharingRecipientsResponse> server_response =
      pending_request_->TakeResponse();
  // Destroy the request object after the response was fetched otherwise no
  // further call can be made.
  pending_request_.reset();

  CHECK(server_response.has_value());

  switch (server_response->result()) {
    case PasswordSharingRecipientsResponse::SUCCESS: {
      std::vector<RecipientInfo> recipients;
      for (const auto& recipient : server_response->recipients()) {
        recipients.push_back(ToRecipientInfo(recipient));
      }
      FetchFamilyMembersRequestStatus status =
          recipients.empty()
              ? FetchFamilyMembersRequestStatus::kNoOtherFamilyMembers
              : FetchFamilyMembersRequestStatus::kSuccess;
      std::move(callback_).Run(std::move(recipients), status);
      return;
    }
    case PasswordSharingRecipientsResponse::NOT_FAMILY_MEMBER: {
      std::move(callback_).Run(std::vector<RecipientInfo>(),
                               FetchFamilyMembersRequestStatus::kNoFamily);
      return;
    }
    case PasswordSharingRecipientsResponse::UNKNOWN: {
      std::move(callback_).Run(std::vector<RecipientInfo>(),
                               FetchFamilyMembersRequestStatus::kUnknown);
      return;
    }
  }
}

}  // namespace password_manager
