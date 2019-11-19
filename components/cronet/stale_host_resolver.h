// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_STALE_HOST_RESOLVER_H_
#define COMPONENTS_CRONET_STALE_HOST_RESOLVER_H_

#include <memory>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "base/time/default_tick_clock.h"
#include "net/base/completion_once_callback.h"
#include "net/dns/host_resolver.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {
class ContextHostResolver;
}  // namespace net

namespace cronet {
namespace {
class StaleHostResolverTest;
}  // namespace

// A HostResolver that wraps a ContextHostResolver and uses it to make requests,
// but "impatiently" returns stale data (if available and usable) after a delay,
// to reduce DNS latency at the expense of accuracy.
class StaleHostResolver : public net::HostResolver {
 public:
  struct StaleOptions {
    StaleOptions();

    // How long to wait before returning stale data, if available.
    base::TimeDelta delay;

    // If positive, how long stale data can be past the expiration time before
    // it's considered unusable. If zero or negative, stale data can be used
    // indefinitely.
    base::TimeDelta max_expired_time;

    // If set, stale data from previous networks is usable; if clear, it's not.
    //
    // If the other network had a working, correct DNS setup, this can increase
    // the availability of useful stale results.
    //
    // If the other network had a broken (e.g. hijacked for captive portal) DNS
    // setup, this will instead end up returning useless results.
    bool allow_other_network;

    // If positive, the maximum number of times a stale entry can be used. If
    // zero, there is no limit.
    int max_stale_uses;

    // If network resolution returns ERR_NAME_NOT_RESOLVED, use stale result if
    // available.
    bool use_stale_on_name_not_resolved;
  };

  // Creates a StaleHostResolver that uses |inner_resolver| for actual
  // resolution, but potentially returns stale data according to
  // |stale_options|.
  StaleHostResolver(std::unique_ptr<net::ContextHostResolver> inner_resolver,
                    const StaleOptions& stale_options);

  ~StaleHostResolver() override;

  // HostResolver implementation:

  void OnShutdown() override;

  // Resolves as a regular HostResolver, but if stale data is available and
  // usable (according to the options passed to the constructor), and fresh data
  // is not returned before the specified delay, returns the stale data instead.
  //
  // If stale data is returned, the StaleHostResolver allows the underlying
  // request to continue in order to repopulate the cache.
  std::unique_ptr<ResolveHostRequest> CreateRequest(
      const net::HostPortPair& host,
      const net::NetworkIsolationKey& network_isolation_key,
      const net::NetLogWithSource& net_log,
      const base::Optional<ResolveHostParameters>& optional_parameters)
      override;

  // The remaining public methods pass through to the inner resolver:

  net::HostCache* GetHostCache() override;
  std::unique_ptr<base::Value> GetDnsConfigAsValue() const override;
  void SetRequestContext(net::URLRequestContext* request_context) override;

 private:
  class RequestImpl;
  friend class StaleHostResolverTest;

  // Called on completion of |network_request| when completed asynchronously (a
  // "network" request). Determines if the request is owned by a RequestImpl or
  // if it is a detached request and handles appropriately.
  void OnNetworkRequestComplete(ResolveHostRequest* network_request,
                                base::WeakPtr<RequestImpl> stale_request,
                                int error);

  // Detach an inner request from a RequestImpl, letting it finish (and populate
  // the host cache) as long as |this| is not destroyed.
  void DetachRequest(std::unique_ptr<ResolveHostRequest> request);

  // Set |tick_clock_| for testing. Must be set before issuing any requests.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // The underlying ContextHostResolver that will be used to make cache and
  // network requests.
  std::unique_ptr<net::ContextHostResolver> inner_resolver_;

  // Shared instance of tick clock, overridden for testing.
  const base::TickClock* tick_clock_ = base::DefaultTickClock::GetInstance();

  // Options that govern when a stale response can or can't be returned.
  const StaleOptions options_;

  // Requests not used for returned results but allowed to continue (unless
  // |this| is destroyed) to backfill the cache.
  std::unordered_map<ResolveHostRequest*, std::unique_ptr<ResolveHostRequest>>
      detached_requests_;

  base::WeakPtrFactory<StaleHostResolver> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StaleHostResolver);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_STALE_HOST_RESOLVER_H_
