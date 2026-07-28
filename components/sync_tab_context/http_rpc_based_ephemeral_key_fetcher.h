// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_HTTP_RPC_BASED_EPHEMERAL_KEY_FETCHER_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_HTTP_RPC_BASED_EPHEMERAL_KEY_FETCHER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/sync_tab_context/ephemeral_key_fetcher.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace sync_tab_context {

// Implementation of `EphemeralKeyFetcher` that fetches an OAuth2 access token
// using `signin::IdentityManager` and issues an HTTP POST RPC to fetch an
// ephemeral key set. Supports multiple concurrent fetch requests.
class HttpRpcBasedEphemeralKeyFetcher : public EphemeralKeyFetcher {
 public:
  using UrlLoaderFactoryGetter =
      base::RepeatingCallback<scoped_refptr<network::SharedURLLoaderFactory>()>;

  HttpRpcBasedEphemeralKeyFetcher(
      signin::IdentityManager* identity_manager,
      UrlLoaderFactoryGetter url_loader_factory_getter,
      const GURL& server_url);
  HttpRpcBasedEphemeralKeyFetcher(const HttpRpcBasedEphemeralKeyFetcher&) =
      delete;
  HttpRpcBasedEphemeralKeyFetcher& operator=(
      const HttpRpcBasedEphemeralKeyFetcher&) = delete;
  ~HttpRpcBasedEphemeralKeyFetcher() override;

  // EphemeralKeyFetcher implementation.
  void FetchEphemeralKey(FetchCallback callback) override;

  // Returns the number of currently ongoing fetch operations.
  size_t ongoing_operations_count_for_testing() const {
    return ongoing_operations_.size();
  }

 private:
  class Operation;

  void OnOperationCompleted(Operation* op,
                            FetchCallback callback,
                            std::optional<Result> result);

  const raw_ptr<signin::IdentityManager> identity_manager_;
  const UrlLoaderFactoryGetter url_loader_factory_getter_;
  const GURL server_url_;

  std::vector<std::unique_ptr<Operation>> ongoing_operations_;
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_HTTP_RPC_BASED_EPHEMERAL_KEY_FETCHER_H_
