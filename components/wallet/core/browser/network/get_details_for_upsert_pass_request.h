// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_NETWORK_GET_DETAILS_FOR_UPSERT_PASS_REQUEST_H_
#define COMPONENTS_WALLET_CORE_BROWSER_NETWORK_GET_DETAILS_FOR_UPSERT_PASS_REQUEST_H_

#include <string>

#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "components/wallet/core/browser/network/wallet_request.h"
#include "components/wallet/core/browser/proto/api_v1.pb.h"

namespace wallet {

// Request to retrieve details (legal message and context token) needed prior
// to upserting a public pass. If the user is not eligible for legal
// disclosures (e.g. non-US regions), `PassUpsertDetails::context_token` and
// `PassUpsertDetails::legal_message` will be `std::nullopt`.
class GetDetailsForUpsertPassRequest : public WalletRequest {
 public:
  GetDetailsForUpsertPassRequest(
      WalletHttpClient::PassType pass_type,
      WalletHttpClient::GetDetailsForUpsertPassCallback callback);
  ~GetDetailsForUpsertPassRequest() override;

  // WalletRequest:
  std::string GetRequestUrlPath() const override;
  std::string GetRequestContent() const override;
  WalletNetworkRequestType GetRequestType() const override;
  base::TimeDelta GetTimeout() const override;
  void OnResponse(WalletHttpClient::HttpResponse http_response) && override;

 private:
  WalletHttpClient::PassType pass_type_;
  WalletHttpClient::GetDetailsForUpsertPassCallback callback_;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_NETWORK_GET_DETAILS_FOR_UPSERT_PASS_REQUEST_H_
