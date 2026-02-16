// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_REQUEST_H_
#define COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "components/wallet/core/browser/proto/client_info.pb.h"
#include "net/http/http_request_headers.h"

namespace wallet {

// Base class for the various Wallet request types.
class WalletRequest {
 public:
  using WalletResponseCallback =
      base::OnceCallback<void(WalletHttpClient::HttpResponse)>;

  // Defined for use in metrics and to share code for certain network-requests.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(WalletNetworkRequestType)
  enum class WalletNetworkRequestType {
    kUpsertPass = 0,
    kUpsertPrivatePass = 1,
    kGetUnmaskedPrivatePass = 2,
    kMaxValue = kGetUnmaskedPrivatePass,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/wallet/histograms.xml:Wallet.NetworkRequest.RequestType)

  virtual ~WalletRequest() = default;

  // Returns the URL path for this type of request.
  virtual std::string GetRequestUrlPath() const = 0;

  // Returns the content that should be provided in the HTTP request.
  virtual std::string GetRequestContent() const = 0;

  // Returns the HTTP request headers that should be provided for this request.
  virtual net::HttpRequestHeaders GetRequestHeaders() const;

  // Returns the type of the request.
  virtual WalletNetworkRequestType GetRequestType() const = 0;

  // Handles the response from the server.
  virtual void OnResponse(WalletHttpClient::HttpResponse http_response) && = 0;

 protected:
  // Returns the client information that should be provided in Wallet requests.
  static ClientInfo BuildClientInfo();
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_REQUEST_H_
