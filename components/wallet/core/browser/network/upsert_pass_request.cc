// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/upsert_pass_request.h"

#include "base/json/json_writer.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace wallet {

namespace {

constexpr int kExternalIdNamespaceChrome = 1;

base::DictValue BuildExternalId() {
  return base::DictValue()
      .Set("namespace", kExternalIdNamespaceChrome)
      .Set("external_id", base::Uuid::GenerateRandomV4().AsLowercaseString());
}

base::DictValue BuildClientInfo() {
  base::DictValue chrome_client_info =
      base::DictValue().Set("version", version_info::GetVersionNumber());

  return base::DictValue().Set("chrome_client_info",
                               std::move(chrome_client_info));
}

base::DictValue BuildLoyaltyCardRequest(const LoyaltyCard& card) {
  return base::DictValue().Set("loyalty_card",
                               base::DictValue()
                                   .Set("merchant_name", card.issuer_name)
                                   .Set("loyalty_number", card.member_id)
                                   .Set("program_name", card.plan_name));
}

base::DictValue BuildPassDict(const WalletPass& pass) {
  base::DictValue response;
  if (pass.id) {
    response.Set("pass_id", *pass.id);
  }
  response.Set("external_id", BuildExternalId());
  response.Merge(std::visit(
      absl::Overload{
          [](const LoyaltyCard& card) { return BuildLoyaltyCardRequest(card); },
          [](const EventPass& pass) {
            NOTIMPLEMENTED();
            return base::DictValue();
          },
          [](const BoardingPass& pass) {
            NOTIMPLEMENTED();
            return base::DictValue();
          },
          [](const TransitTicket& ticket) {
            NOTIMPLEMENTED();
            return base::DictValue();
          },
          [](const auto&) {
            NOTREACHED();
            return base::DictValue();
          }},
      pass.pass_data));
  return response;
}

}  // namespace

UpsertPassRequest::UpsertPassRequest(
    WalletPass pass,
    WalletHttpClient::UpsertPassCallback callback)
    : pass_(std::move(pass)), callback_(std::move(callback)) {
  CHECK(callback_);
}

UpsertPassRequest::~UpsertPassRequest() = default;

std::string UpsertPassRequest::GetRequestUrlPath() const {
  return "v1/passes:upsert";
}

std::string UpsertPassRequest::GetRequestContent() const {
  const base::DictValue request_dict =
      base::DictValue()
          .Set("pass", BuildPassDict(pass_))
          .Set("client_info", BuildClientInfo());
  return base::WriteJson(request_dict).value_or("");
}

void UpsertPassRequest::OnResponse(
    WalletHttpClient::HttpResponse http_response) && {
  if (!http_response.has_value()) {
    std::move(callback_).Run(base::unexpected(http_response.error()));
    return;
  }

  // TODO(crbug.com/468916773): Parse the response body to extract the pass.
  std::move(callback_).Run(WalletPass{});
}

}  // namespace wallet
