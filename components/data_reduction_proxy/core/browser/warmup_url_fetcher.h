// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_WARMUP_URL_FETCHER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_WARMUP_URL_FETCHER_H_

#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/proxy_server.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

class GURL;

namespace net {
struct RedirectInfo;
}  // namespace net

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace data_reduction_proxy {
class DataReductionProxyServer;

class WarmupURLFetcher {
 public:
  enum class FetchResult { kFailed, kSuccessful, kTimedOut };

  // The proxy server that was used to fetch the request, and whether the fetch
  // was successful.
  using WarmupURLFetcherCallback =
      base::RepeatingCallback<void(const net::ProxyServer&, FetchResult)>;

  // Callback to obtain the current HTTP RTT estimate.
  using GetHttpRttCallback =
      base::RepeatingCallback<base::Optional<base::TimeDelta>()>;

  // Callback to get a config for the given proxy servers.
  using CreateCustomProxyConfigCallback =
      base::RepeatingCallback<network::mojom::CustomProxyConfigPtr(
          const std::vector<DataReductionProxyServer>&)>;

  WarmupURLFetcher(
      CreateCustomProxyConfigCallback create_custom_proxy_config_callback,
      WarmupURLFetcherCallback callback,
      GetHttpRttCallback get_http_rtt_callback,
      const std::string& user_agent);

  virtual ~WarmupURLFetcher();

  // Creates and starts a URLLoader that loads the warmup URL.
  // |previous_attempt_counts| is the count of fetch attempts that have been
  // made to the proxy which is being probed. The fetching may happen after some
  // delay depending on |previous_attempt_counts|.
  void FetchWarmupURL(size_t previous_attempt_counts,
                      const DataReductionProxyServer& proxy_server);

  // Returns true if a warmup URL fetch is currently in-flight.
  bool IsFetchInFlight() const;

 protected:
  // Sets |warmup_url_with_query_params| to the warmup URL. Attaches random
  // query params to the warmup URL.
  void GetWarmupURLWithQueryParam(GURL* warmup_url_with_query_params) const;

  // Returns the time for which the fetching of the warmup URL probe should be
  // delayed.
  virtual base::TimeDelta GetFetchWaitTime() const;

  // Returns the timeout value for fetching the secure proxy URL. Virtualized
  // for testing.
  virtual base::TimeDelta GetFetchTimeout() const;

  // Called when the fetch timeouts.
  void OnFetchTimeout();

  // URL loader callback when response starts.
  void OnURLLoadResponseStarted(
      const GURL& final_url,
      const network::mojom::URLResponseHead& response_head);

  // URL loader callback for redirections.
  void OnURLLoaderRedirect(const net::RedirectInfo& redirect_info,
                           const network::mojom::URLResponseHead& response_head,
                           std::vector<std::string>* to_be_removed_headers);

  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // The URLLoader being used for loading the warmup URL. Protected for
  // testing.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Timer used to delay the fetching of the warmup probe URL.
  base::OneShotTimer fetch_delay_timer_;

  // Timer to enforce timeout of fetching the warmup URL.
  base::OneShotTimer fetch_timeout_timer_;

  // True if the loader for warmup URL is in-flight.
  bool is_fetch_in_flight_;

  // Proxy server used on the last resource loaded.
  net::ProxyServer proxy_server_;

 private:
  // Creates and immediately starts a URLLoader that fetches the warmup URL.
  void FetchWarmupURLNow(const DataReductionProxyServer& proxy_server);

  // Resets the variable after the fetching of the warmup URL has completed or
  // timed out. Must be called after |callback_| has been run.
  void CleanupAfterFetch();

  // Gets a URLLoaderFactory which will only attempt to use |proxy_server| as a
  // proxy.
  virtual network::mojom::URLLoaderFactory* GetNetworkServiceURLLoaderFactory(
      const DataReductionProxyServer& proxy_server);

  // Count of fetch attempts that have been made to the proxy which is being
  // probed.
  size_t previous_attempt_counts_;

  CreateCustomProxyConfigCallback create_custom_proxy_config_callback_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  mojo::Remote<network::mojom::NetworkContext> context_;

  // Callback that should be executed when the fetching of the warmup URL is
  // completed.
  WarmupURLFetcherCallback callback_;

  // Callback to obtain the current HTTP RTT estimate.
  GetHttpRttCallback get_http_rtt_callback_;

  const std::string user_agent_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(WarmupURLFetcher);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_WARMUP_URL_FETCHER_H_
