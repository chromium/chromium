// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/upsert_private_pass_request.h"

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "components/wallet/core/browser/proto/api_v1.pb.h"
#include "components/wallet/core/browser/proto/client_info.pb.h"
#include "components/wallet/core/browser/proto/private_pass.pb.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace wallet {
namespace {

// Token ID for wallet private pass data.
constexpr int kWalletPrivatePassData = 574;

// Builds the EES-Proto-Tokenization header value for the given pass.
// The header contains a comma-separated list of field paths and their
// corresponding token IDs (SecureDataType).For more details on the
// implementation and backwards compatibility, see: http://shortn/_cLCEg7EN0k
std::string BuildEESTokenizationForPass(const PrivatePass& pass) {
  const int root_tag = api::UpsertPrivatePassRequest::kPrivatePassFieldNumber;
  switch (pass.data_case()) {
    case PrivatePass::kDriverLicense:
      return base::StringPrintf(
          "%d.%d.%d;%d", root_tag, PrivatePass::kDriverLicenseFieldNumber,
          PrivatePass::DriverLicense::kDriverLicenseNumberFieldNumber,
          kWalletPrivatePassData);
    case PrivatePass::kPassport:
      return base::StringPrintf(
          "%d.%d.%d;%d", root_tag, PrivatePass::kPassportFieldNumber,
          PrivatePass::Passport::kPassportNumberFieldNumber,
          kWalletPrivatePassData);
    case PrivatePass::kIdCard:
      return base::StringPrintf(
          "%d.%d.%d;%d", root_tag, PrivatePass::kIdCardFieldNumber,
          PrivatePass::IdCard::kIdNumberFieldNumber, kWalletPrivatePassData);
    case PrivatePass::kRedressNumber:
      return base::StringPrintf(
          "%d.%d.%d;%d", root_tag, PrivatePass::kRedressNumberFieldNumber,
          PrivatePass::RedressNumber::kRedressNumberFieldNumber,
          kWalletPrivatePassData);
    case PrivatePass::kKnownTravelerNumber:
      return base::StringPrintf(
          "%d.%d.%d;%d", root_tag, PrivatePass::kKnownTravelerNumberFieldNumber,
          PrivatePass::KnownTravelerNumber::kKnownTravelerNumberFieldNumber,
          kWalletPrivatePassData);
    case PrivatePass::DATA_NOT_SET:
      NOTREACHED();
  }
}

}  // namespace

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

net::HttpRequestHeaders UpsertPrivatePassRequest::GetRequestHeaders() const {
  net::HttpRequestHeaders headers;
  headers.SetHeader("EES-S7E-Mode", "proto");
  headers.SetHeader("EES-Proto-Tokenization",
                    BuildEESTokenizationForPass(pass_));
  return headers;
}

WalletRequest::WalletNetworkRequestType
UpsertPrivatePassRequest::GetRequestType() const {
  return WalletRequest::WalletNetworkRequestType::kUpsertPrivatePass;
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
