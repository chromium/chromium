// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/get_details_for_upsert_pass_request.h"

#include <utility>

#include "base/check.h"
#include "base/time/time.h"
#include "components/wallet/core/browser/proto/api_v1.pb.h"

namespace wallet {

namespace {

api::GetDetailsForUpsertPassRequest::PassType ToProtoPassType(
    WalletHttpClient::PassType pass_type) {
  switch (pass_type) {
    case WalletHttpClient::PassType::kVehicleRegistration:
      return api::GetDetailsForUpsertPassRequest::
          PASS_TYPE_VEHICLE_REGISTRATION;
    case WalletHttpClient::PassType::kLoyaltyCard:
      return api::GetDetailsForUpsertPassRequest::PASS_TYPE_LOYALTY_CARD;
    case WalletHttpClient::PassType::kUnspecified:
      return api::GetDetailsForUpsertPassRequest::PASS_TYPE_UNSPECIFIED;
  }
}

}  // namespace

GetDetailsForUpsertPassRequest::GetDetailsForUpsertPassRequest(
    WalletHttpClient::PassType pass_type,
    WalletHttpClient::GetDetailsForUpsertPassCallback callback)
    : pass_type_(pass_type), callback_(std::move(callback)) {
  CHECK(callback_);
}

GetDetailsForUpsertPassRequest::~GetDetailsForUpsertPassRequest() = default;

std::string GetDetailsForUpsertPassRequest::GetRequestUrlPath() const {
  return "v1/passes:getDetailsForUpsert";
}

std::string GetDetailsForUpsertPassRequest::GetRequestContent() const {
  api::GetDetailsForUpsertPassRequest request;
  *request.mutable_client_info() = BuildClientInfo();
  request.set_pass_type(ToProtoPassType(pass_type_));
  return request.SerializeAsString();
}

WalletRequest::WalletNetworkRequestType
GetDetailsForUpsertPassRequest::GetRequestType() const {
  return WalletRequest::WalletNetworkRequestType::kGetDetailsForUpsertPass;
}

base::TimeDelta GetDetailsForUpsertPassRequest::GetTimeout() const {
  return base::Milliseconds(6500);
}

void GetDetailsForUpsertPassRequest::OnResponse(
    WalletHttpClient::HttpResponse http_response) && {
  if (!http_response.has_value()) {
    std::move(callback_).Run(base::unexpected(http_response.error()));
    return;
  }

  api::GetDetailsForUpsertPassResponse response;
  if (!response.ParseFromString(http_response.value())) {
    std::move(callback_).Run(base::unexpected(
        WalletHttpClient::WalletRequestError::kParseResponseFailed));
    return;
  }

  // The server omits `legal_message` and `context_token` if the user/account is
  // not eligible for disclosures (e.g. non-US regions or missing capabilities).
  // In this case, both fields are `std::nullopt`, and the client proceeds with
  // the regular save flow without legal disclosures.
  std::optional<std::string> context_token;
  if (response.has_context_token()) {
    context_token = response.context_token();
  }

  std::optional<LegalMessage> legal_message;
  if (response.has_legal_message()) {
    legal_message = std::move(*response.mutable_legal_message());
  }

  std::move(callback_).Run(WalletHttpClient::PassUpsertDetails{
      .context_token = std::move(context_token),
      .legal_message = std::move(legal_message),
  });
}

}  // namespace wallet
