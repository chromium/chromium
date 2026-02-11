// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/upsert_public_pass_request.h"

#include "base/json/json_writer.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/wallet/core/browser/proto/api_v1.pb.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace wallet {

UpsertPublicPassRequest::UpsertPublicPassRequest(
    Pass pass,
    WalletHttpClient::UpsertPublicPassCallback callback)
    : pass_(std::move(pass)), callback_(std::move(callback)) {
  CHECK(callback_);
}

UpsertPublicPassRequest::~UpsertPublicPassRequest() = default;

std::string UpsertPublicPassRequest::GetRequestUrlPath() const {
  return "v1/passes:upsert";
}

std::string UpsertPublicPassRequest::GetRequestContent() const {
  api::UpsertPassRequest request;
  *request.mutable_pass() = pass_;
  *request.mutable_client_info() = BuildClientInfo();
  return request.SerializeAsString();
}

WalletRequest::WalletNetworkRequestType
UpsertPublicPassRequest::GetRequestType() const {
  return WalletRequest::WalletNetworkRequestType::kUpsertPass;
}

void UpsertPublicPassRequest::OnResponse(
    WalletHttpClient::HttpResponse http_response) && {
  if (!http_response.has_value()) {
    std::move(callback_).Run(base::unexpected(http_response.error()));
    return;
  }

  api::UpsertPassResponse response;
  if (!response.ParseFromString(http_response.value())) {
    std::move(callback_).Run(base::unexpected(
        WalletHttpClient::WalletRequestError::kParseResponseFailed));
    return;
  }

  std::move(callback_).Run(std::move(*response.mutable_pass_id()));
}

}  // namespace wallet
