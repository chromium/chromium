// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_NETWORKING_HOST_H_
#define COMPONENTS_FEED_CORE_FEED_NETWORKING_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class TickClock;
}

namespace signin {
class IdentityManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace feed {

class NetworkFetch;

// Implementation of the Feed Networking Host API. The networking host handles
// the process of sending http requests to remote endpoints, including the
// fetching of access tokens for the primary user.
class FeedNetworkingHost {
 public:
  // status_code is a union of net::Error (if the request failed) and the http
  // status code(if the request succeeded in reaching the server).
  using ResponseCallback =
      base::OnceCallback<void(int32_t status_code,
                              std::vector<uint8_t> response_bytes)>;

  FeedNetworkingHost(
      signin::IdentityManager* identity_manager,
      const std::string& api_key,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      const base::TickClock* tick_clock,
      PrefService* pref_service);

  ~FeedNetworkingHost();

  // Cancels all pending requests immediately. This could be used, for example,
  // if there are pending requests for a user who just signed out.
  void CancelRequests();

  // Start a request to |url| of type |request_type| with body |request_body|.
  // |callback| will be called when the response is received or if there is
  // an error, including non-protocol errors. The contents of |request_body|
  // will be gzipped.
  void Send(
      const GURL& url,
      const std::string& request_type,
      std::vector<uint8_t> request_body,
      ResponseCallback callback);

 private:
  void NetworkFetchFinished(NetworkFetch* fetch,
                            ResponseCallback callback,
                            int32_t http_code,
                            std::vector<uint8_t> response_body);

  base::flat_set<std::unique_ptr<NetworkFetch>, base::UniquePtrComparator>
      pending_requests_;
  signin::IdentityManager* identity_manager_;
  const std::string api_key_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  const base::TickClock* tick_clock_;
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(FeedNetworkingHost);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_NETWORKING_HOST_H_
