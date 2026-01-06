// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_HTTP_CLIENT_IMPL_H_
#define COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_HTTP_CLIENT_IMPL_H_

#include <list>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace wallet {

class WalletHttpClientImpl : public WalletHttpClient {
 public:
  explicit WalletHttpClientImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~WalletHttpClientImpl() override;

  WalletHttpClientImpl(const WalletHttpClientImpl&) = delete;
  WalletHttpClientImpl& operator=(const WalletHttpClientImpl&) = delete;

  // WalletHttpClient:
  void SavePass(const WalletablePass& pass, SavePassCallback callback) override;

 private:
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;
  using HttpResponse = base::expected<std::string, WalletRequestError>;

  void SendRequest(const std::string& request_path,
                   const std::string& request_body,
                   base::OnceCallback<void(HttpResponse)> response_callback);

  // Called when the `SimpleURLLoader` referenced by `it` completes. Removes the
  // loader from `active_loaders_` and runs `response_callback` with the
  // response body or an error.
  void OnSimpleLoaderComplete(
      UrlLoaderList::iterator it,
      base::OnceCallback<void(HttpResponse)> response_callback,
      std::optional<std::string> response_body);

  // Parses `http_response` and runs `callback` with the result.
  void OnSavePassResponse(SavePassCallback callback,
                          HttpResponse http_response);

  // The factory used to create URLLoaders.
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Owns the ongoing SimpleURLLoaders, keeping them alive until completion.
  UrlLoaderList active_loaders_;

  base::WeakPtrFactory<WalletHttpClientImpl> weak_ptr_factory_{this};
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_HTTP_CLIENT_IMPL_H_
