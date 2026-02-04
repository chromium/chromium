// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_REQUEST_H_
#define COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"

namespace wallet {

// Base class for the various Wallet request types.
class WalletRequest {
 public:
  using WalletResponseCallback =
      base::OnceCallback<void(WalletHttpClient::HttpResponse)>;

  virtual ~WalletRequest() = default;

  // Returns the URL path for this type of request.
  virtual std::string GetRequestUrlPath() const = 0;

  // Returns the content that should be provided in the HTTP request.
  virtual std::string GetRequestContent() const = 0;

  // Handles the response from the server.
  virtual void OnResponse(WalletHttpClient::HttpResponse http_response) && = 0;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_REQUEST_H_
