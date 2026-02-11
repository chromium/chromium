// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/get_unmasked_pass_request.h"

#include "base/notimplemented.h"
#include "components/wallet/core/browser/proto/api_v1.pb.h"

namespace wallet {

GetUnmaskedPassRequest::GetUnmaskedPassRequest(
    std::string pass_id,
    WalletHttpClient::GetUnmaskedPassCallback callback)
    : pass_id_(pass_id), callback_(std::move(callback)) {
  CHECK(callback_);
}

GetUnmaskedPassRequest::~GetUnmaskedPassRequest() = default;

std::string GetUnmaskedPassRequest::GetRequestUrlPath() const {
  return "v1/e/privatePasses:batchGet";
}

std::string GetUnmaskedPassRequest::GetRequestContent() const {
  api::GetPrivatePassesRequest request;
  request.add_pass_ids(pass_id_);
  *request.mutable_client_info() = BuildClientInfo();
  return request.SerializeAsString();
}

WalletRequest::WalletNetworkRequestType GetUnmaskedPassRequest::GetRequestType()
    const {
  return WalletRequest::WalletNetworkRequestType::kGetUnmaskedPrivatePass;
}

void GetUnmaskedPassRequest::OnResponse(
    WalletHttpClient::HttpResponse http_response) && {
  if (!http_response.has_value()) {
    std::move(callback_).Run(base::unexpected(http_response.error()));
    return;
  }

  api::GetPrivatePassesResponse response;
  if (!response.ParseFromString(http_response.value())) {
    std::move(callback_).Run(base::unexpected(
        WalletHttpClient::WalletRequestError::kParseResponseFailed));
    return;
  }

  if (response.results_size() == 0) {
    std::move(callback_).Run(
        base::unexpected(WalletHttpClient::WalletRequestError::kGenericError));
    return;
  }

  // Since we only requested one pass, we only care about the first result.
  api::GetPrivatePassResult* result = response.mutable_results(0);
  if (result->has_error()) {
    // TODO(crbug.com/468915960): Map RpcStatus to WalletRequestError.
    std::move(callback_).Run(
        base::unexpected(WalletHttpClient::WalletRequestError::kGenericError));
    return;
  }

  if (!result->has_private_pass()) {
    std::move(callback_).Run(
        base::unexpected(WalletHttpClient::WalletRequestError::kGenericError));
    return;
  }

  std::move(callback_).Run(std::move(*result->mutable_private_pass()));
}

}  // namespace wallet
