// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/upsert_private_pass_request.h"

#include "base/json/json_writer.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "components/wallet/core/browser/proto/api_v1.pb.h"
#include "components/wallet/core/browser/proto/client_info.pb.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace wallet {

UpsertPrivatePassRequest::UpsertPrivatePassRequest(
    PrivatePass pass,
    WalletHttpClient::UpsertPrivatePassCallback callback)
    : pass_(std::move(pass)), callback_(std::move(callback)) {
  CHECK(callback_);
}

UpsertPrivatePassRequest::~UpsertPrivatePassRequest() = default;

std::string UpsertPrivatePassRequest::GetRequestUrlPath() const {
  return "v1/e/privatePasses:upsert";
}

std::string UpsertPrivatePassRequest::GetRequestContent() const {
  api::UpsertPrivatePassRequest request;
  *request.mutable_private_pass() = pass_;
  *request.mutable_client_info() = BuildClientInfo();
  return request.SerializeAsString();
}

void UpsertPrivatePassRequest::OnResponse(
    WalletHttpClient::HttpResponse http_response) && {
  if (!http_response.has_value()) {
    std::move(callback_).Run(base::unexpected(http_response.error()));
    return;
  }

  api::UpsertPrivatePassResponse response;
  if (!response.ParseFromString(http_response.value())) {
    std::move(callback_).Run(base::unexpected(
        WalletHttpClient::WalletRequestError::kParseResponseFailed));
    return;
  }

  std::move(callback_).Run(std::move(*response.mutable_private_pass()));
}

}  // namespace wallet
