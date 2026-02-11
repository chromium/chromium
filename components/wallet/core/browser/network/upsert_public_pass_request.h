// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_NETWORK_UPSERT_PUBLIC_PASS_REQUEST_H_
#define COMPONENTS_WALLET_CORE_BROWSER_NETWORK_UPSERT_PUBLIC_PASS_REQUEST_H_

#include <string>

#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "components/wallet/core/browser/network/wallet_request.h"
#include "components/wallet/core/browser/proto/pass.pb.h"

namespace wallet {

class UpsertPublicPassRequest : public WalletRequest {
 public:
  UpsertPublicPassRequest(Pass pass,
                          WalletHttpClient::UpsertPublicPassCallback callback);
  ~UpsertPublicPassRequest() override;

  // WalletRequest:
  std::string GetRequestUrlPath() const override;
  std::string GetRequestContent() const override;
  WalletNetworkRequestType GetRequestType() const override;
  void OnResponse(WalletHttpClient::HttpResponse http_response) && override;

 private:
  const Pass pass_;
  WalletHttpClient::UpsertPublicPassCallback callback_;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_NETWORK_UPSERT_PUBLIC_PASS_REQUEST_H_
