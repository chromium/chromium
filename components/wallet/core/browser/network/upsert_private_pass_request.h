// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_NETWORK_UPSERT_PRIVATE_PASS_REQUEST_H_
#define COMPONENTS_WALLET_CORE_BROWSER_NETWORK_UPSERT_PRIVATE_PASS_REQUEST_H_

#include <string>

#include "components/consent_auditor/consent_auditor.h"
#include "components/wallet/core/browser/data_models/wallet_pass.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "components/wallet/core/browser/network/wallet_request.h"
#include "components/wallet/core/browser/proto/private_pass.pb.h"

namespace wallet {

class UpsertPrivatePassRequest : public WalletRequest {
 public:
  UpsertPrivatePassRequest(
      PrivatePass pass,
      std::optional<consent_auditor::ConsentAuditor::SessionId> session_id,
      WalletHttpClient::UpsertPrivatePassCallback callback);
  ~UpsertPrivatePassRequest() override;

  // WalletRequest:
  std::string GetRequestUrlPath() const override;
  std::string GetRequestContent() const override;
  net::HttpRequestHeaders GetRequestHeaders() const override;
  WalletNetworkRequestType GetRequestType() const override;
  base::TimeDelta GetTimeout() const override;
  void OnResponse(WalletHttpClient::HttpResponse http_response) && override;

 private:
  const PrivatePass pass_;
  // Set for Upsert requests that correspond to the creation of new passes,
  // indicated by the absence of `pass_.id`.
  const std::optional<consent_auditor::ConsentAuditor::SessionId> session_id_;
  WalletHttpClient::UpsertPrivatePassCallback callback_;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_NETWORK_UPSERT_PRIVATE_PASS_REQUEST_H_
