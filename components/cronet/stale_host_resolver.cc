// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/stale_host_resolver.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/dns_util.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/net_log_with_source.h"
#include "url/scheme_host_port.h"

namespace cronet {

// A request made by the StaleHostResolver. May return fresh cached data,
// network data, or stale cached data.
class StaleHostResolver::RequestImpl
    : public net::HostResolver::ResolveHostRequest {
 public:
  // StaleOptions will be read directly from |resolver|.
  RequestImpl(base::WeakPtr<StaleHostResolver> resolver,
              const net::HostPortPair& host,
              const net::NetworkAnonymizationKey& network_anonymization_key,
              const net::NetLogWithSource& net_log,
              const ResolveHostParameters& input_parameters,
              const base::TickClock* tick_clock);
  ~RequestImpl() override = default;

  // net::HostResolver::ResolveHostRequest implementation:
  int Start(net::CompletionOnceCallback result_callback) override;
  const net::AddressList* GetAddressResults() const override;
  const net::HostResolverEndpointResults* GetEndpointResults() const override;
  const std::vector<std::string>* GetTextResults() const override;
  const std::vector<net::HostPortPair>* GetHostnameResults() const override;
  const std::set<std::string>* GetDnsAliasResults() const override;
  net::ResolveErrorInfo GetResolveErrorInfo() const override;
  const std::optional<net::HostCache::EntryStaleness>& GetStaleInfo()
      const override;
  void ChangeRequestPriority(net::RequestPriority priority) override;

  // Called on completion of an asynchronous (network) inner request. Expected
  // to be called by StaleHostResolver::OnNetworkRequestComplete().
  void OnNetworkRequestComplete(int error);

 private:
  bool have_network_request() const { return network_request_ != nullptr; }
  bool have_cache_data() const {
    return cache_error_ != net::ERR_DNS_CACHE_MISS;
  }
  bool have_returned() const { return result_callback_.is_null(); }

  // Determines if |cache_error_| and |cache_request_| represents a usable entry
  // per the requirements of |resolver_->options_|.
  bool CacheDataIsUsable() const;

  // Callback for |stale_timer_| that returns stale results.
  void OnStaleDelayElapsed();

  base::WeakPtr<StaleHostResolver> resolver_;

  const net::HostPortPair host_;
  const net::NetworkAnonymizationKey network_anonymization_key_;
  const net::NetLogWithSource net_log_;
  const ResolveHostParameters input_parameters_;

  // The callback passed into |Start()| to be called when the request returns.
  net::CompletionOnceCallback result_callback_;

  // The error from the stale cache entry, if there was one.
  // If not, net::ERR_DNS_CACHE_MISS.
  int cache_error_;
  // Inner local-only/stale-allowed request.
  std::unique_ptr<ResolveHostRequest> cache_request_;
  // A timer that fires when the |Request| should return stale results, if the
  // underlying network request has not finished yet.
  base::OneShotTimer stale_timer_;

  // An inner request for network results. Only set if |cache_request_| gave a
  // stale or unusable result, and unset if the stale result is to be used as
  // the overall result.
  std::unique_ptr<ResolveHostRequest> network_request_;

  base::WeakPtrFactory<RequestImpl> weak_ptr_factory_{this};
};

StaleHostResolver::RequestImpl::RequestImpl(
    base::WeakPtr<StaleHostResolver> resolver,
    const net::HostPortPair& host,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const net::NetLogWithSource& net_log,
    const ResolveHostParameters& input_parameters,
    const base::TickClock* tick_clock)
    : resolver_(std::move(resolver)),
      host_(host),
      network_anonymization_key_(network_anonymization_key),
      net_log_(net_log),
      input_parameters_(input_parameters),
      cache_error_(net::ERR_DNS_CACHE_MISS),
      stale_timer_(tick_clock) {
  DCHECK(resolver_);
}

int StaleHostResolver::RequestImpl::Start(
    net::CompletionOnceCallback result_callback) {
  DCHECK(resolver_);
  DCHECK(!result_callback.is_null());

  net::HostResolver::ResolveHostParameters cache_parameters = input_parameters_;
  cache_parameters.cache_usage =
      net::HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;
  cache_parameters.source = net::HostResolverSource::LOCAL_ONLY;
  cache_request_ = resolver_->inner_resolver_->CreateRequest(
      host_, network_anonymization_key_, net_log_, cache_parameters);
  int error = cache_request_->Start(
      base::BindOnce([](int error) { NOTREACHED_IN_MIGRATION(); }));
  DCHECK_NE(net::ERR_IO_PENDING, error);
  cache_error_ = cache_request_->GetResolveErrorInfo().error;
  DCHECK_NE(net::ERR_IO_PENDING, cache_error_);
  // If it's a fresh cache hit (or literal), return it synchronously.
  if (cache_error_ != net::ERR_DNS_CACHE_MISS &&
      (!cache_request_->GetStaleInfo() ||
       !cache_request_->GetStaleInfo().value().is_stale())) {
    return cache_error_;
  }

  if (cache_error_ != net::ERR_DNS_CACHE_MISS &&
      input_parameters_.cache_usage ==
          net::HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED) {
    return cache_error_;
  }

  result_callback_ = std::move(result_callback);

  if (CacheDataIsUsable()) {
    // |stale_timer_| is deleted when the Request is deleted, so it's safe to
    // use Unretained here.
    stale_timer_.Start(
        FROM_HERE, resolver_->options_.delay,
        base::BindOnce(&StaleHostResolver::RequestImpl::OnStaleDelayElapsed,
                       base::Unretained(this)));
  } else {
    cache_error_ = net::ERR_DNS_CACHE_MISS;
    cache_request_.reset();
  }

  // Don't check the cache again.
  net::HostResolver::ResolveHostParameters no_cache_parameters =
      input_parameters_;
  no_cache_parameters.cache_usage =
      net::HostResolver::ResolveHostParameters::CacheUsage::DISALLOWED;
  network_request_ = resolver_->inner_resolver_->CreateRequest(
      host_, network_anonymization_key_, net_log_, no_cache_parameters);
  int network_rv = network_request_->Start(
      base::BindOnce(&StaleHostResolver::OnNetworkRequestComplete, resolver_,
                     network_request_.get(), weak_ptr_factory_.GetWeakPtr()));

  // Network resolver has returned synchronously (for example by resolving from
  // /etc/hosts).
  if (network_rv != net::ERR_IO_PENDING) {
    stale_timer_.Stop();
  }
  return network_rv;
}

const net::AddressList* StaleHostResolver::RequestImpl::GetAddressResults()
    const {
  if (network_request_)
    return network_request_->GetAddressResults();

  DCHECK(cache_request_);
  return cache_request_->GetAddressResults();
}

const net::HostResolverEndpointResults*
StaleHostResolver::RequestImpl::GetEndpointResults() const {
  if (network_request_)
    return network_request_->GetEndpointResults();

  DCHECK(cache_request_);
  return cache_request_->GetEndpointResults();
}

const std::vector<std::string>* StaleHostResolver::RequestImpl::GetTextResults()
    const {
  if (network_request_)
    return network_request_->GetTextResults();

  DCHECK(cache_request_);
  return cache_request_->GetTextResults();
}

const std::vector<net::HostPortPair>*
StaleHostResolver::RequestImpl::GetHostnameResults() const {
  if (network_request_)
    return network_request_->GetHostnameResults();

  DCHECK(cache_request_);
  return cache_request_->GetHostnameResults();
}

const std::set<std::string>*
StaleHostResolver::RequestImpl::GetDnsAliasResults() const {
  if (network_request_)
    return network_request_->GetDnsAliasResults();

  DCHECK(cache_request_);
  return cache_request_->GetDnsAliasResults();
}

net::ResolveErrorInfo StaleHostResolver::RequestImpl::GetResolveErrorInfo()
    const {
  if (network_request_)
    return network_request_->GetResolveErrorInfo();
  DCHECK(cache_request_);
  return cache_request_->GetResolveErrorInfo();
}

const std::optional<net::HostCache::EntryStaleness>&
StaleHostResolver::RequestImpl::GetStaleInfo() const {
  if (network_request_)
    return network_request_->GetStaleInfo();

  DCHECK(cache_request_);
  return cache_request_->GetStaleInfo();
}

void StaleHostResolver::RequestImpl::ChangeRequestPriority(
    net::RequestPriority priority) {
  if (network_request_) {
    network_request_->ChangeRequestPriority(priority);
  } else {
    DCHECK(cache_request_);
    cache_request_->ChangeRequestPriority(priority);
  }
}

void StaleHostResolver::RequestImpl::OnNetworkRequestComplete(int error) {
  DCHECK(resolver_);
  DCHECK(have_network_request());
  DCHECK(!have_returned());

  bool return_stale_data_instead_of_network_name_not_resolved =
      resolver_->options_.use_stale_on_name_not_resolved &&
      error == net::ERR_NAME_NOT_RESOLVED && have_cache_data();

  stale_timer_.Stop();

  if (return_stale_data_instead_of_network_name_not_resolved) {
    network_request_.reset();
    std::move(result_callback_).Run(cache_error_);
  } else {
    cache_request_.reset();
    std::move(result_callback_).Run(error);
  }
}

bool StaleHostResolver::RequestImpl::CacheDataIsUsable() const {
  DCHECK(resolver_);
  DCHECK(cache_request_);

  if (cache_error_ != net::OK)
    return false;

  DCHECK(cache_request_->GetStaleInfo());
  const net::HostCache::EntryStaleness& staleness =
      cache_request_->GetStaleInfo().value();

  if (resolver_->options_.max_expired_time != base::TimeDelta() &&
      staleness.expired_by > resolver_->options_.max_expired_time) {
    return false;
  }
  if (resolver_->options_.max_stale_uses > 0 &&
      staleness.stale_hits > resolver_->options_.max_stale_uses) {
    return false;
  }
  if (!resolver_->options_.allow_other_network &&
      staleness.network_changes > 0) {
    return false;
  }
  return true;
}

void StaleHostResolver::RequestImpl::OnStaleDelayElapsed() {
  DCHECK(!have_returned());
  DCHECK(have_cache_data());
  DCHECK(have_network_request());

  // If resolver is destroyed after starting a request, the request is
  // considered cancelled and callbacks must not be invoked. Logging the
  // cancellation will happen on destruction of |this|.
  if (!resolver_) {
    network_request_.reset();
    return;
  }
  DCHECK(CacheDataIsUsable());

  // Detach |network_request_| to allow it to complete and backfill the cache
  // even if |this| is destroyed.
  resolver_->DetachRequest(std::move(network_request_));

  std::move(result_callback_).Run(cache_error_);
}

StaleHostResolver::StaleOptions::StaleOptions()
    : allow_other_network(false),
      max_stale_uses(0),
      use_stale_on_name_not_resolved(false) {}

StaleHostResolver::StaleHostResolver(
    std::unique_ptr<net::ContextHostResolver> inner_resolver,
    const StaleOptions& stale_options)
    : inner_resolver_(std::move(inner_resolver)), options_(stale_options) {
  DCHECK_LE(0, stale_options.max_expired_time.InMicroseconds());
  DCHECK_LE(0, stale_options.max_stale_uses);
}

StaleHostResolver::~StaleHostResolver() = default;

void StaleHostResolver::OnShutdown() {
  inner_resolver_->OnShutdown();
}

std::unique_ptr<net::HostResolver::ResolveHostRequest>
StaleHostResolver::CreateRequest(
    url::SchemeHostPort host,
    net::NetworkAnonymizationKey network_anonymization_key,
    net::NetLogWithSource net_log,
    std::optional<ResolveHostParameters> optional_parameters) {
  // TODO(crbug.com/40181080): Propagate scheme.
  return CreateRequest(net::HostPortPair::FromSchemeHostPort(host),
                       network_anonymization_key, net_log, optional_parameters);
}

std::unique_ptr<net::HostResolver::ResolveHostRequest>
StaleHostResolver::CreateRequest(
    const net::HostPortPair& host,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const net::NetLogWithSource& net_log,
    const std::optional<ResolveHostParameters>& optional_parameters) {
  DCHECK(tick_clock_);
  return std::make_unique<RequestImpl>(
      weak_ptr_factory_.GetWeakPtr(), host, network_anonymization_key, net_log,
      optional_parameters.value_or(ResolveHostParameters()), tick_clock_);
}

std::unique_ptr<net::HostResolver::ServiceEndpointRequest>
StaleHostResolver::CreateServiceEndpointRequest(
    Host host,
    net::NetworkAnonymizationKey network_anonymization_key,
    net::NetLogWithSource net_log,
    ResolveHostParameters parameters) {
  // TODO(crbug.com/335119455): Figure out a plan to support the
  // ServiceEndpointRequest API.
  NOTIMPLEMENTED();
  return nullptr;
}

net::HostCache* StaleHostResolver::GetHostCache() {
  return inner_resolver_->GetHostCache();
}

base::Value::Dict StaleHostResolver::GetDnsConfigAsValue() const {
  return inner_resolver_->GetDnsConfigAsValue();
}

void StaleHostResolver::SetRequestContext(
    net::URLRequestContext* request_context) {
  inner_resolver_->SetRequestContext(request_context);
}

void StaleHostResolver::OnNetworkRequestComplete(
    ResolveHostRequest* network_request,
    base::WeakPtr<RequestImpl> stale_request,
    int error) {
  if (detached_requests_.erase(network_request))
    return;

  // If not a detached request, there should still be an owning RequestImpl.
  // Otherwise the request should have been cancelled and this method never
  // called.
  DCHECK(stale_request);

  stale_request->OnNetworkRequestComplete(error);
}

void StaleHostResolver::DetachRequest(
    std::unique_ptr<ResolveHostRequest> request) {
  DCHECK_EQ(0u, detached_requests_.count(request.get()));
  detached_requests_[request.get()] = std::move(request);
}

void StaleHostResolver::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  inner_resolver_->SetTickClockForTesting(tick_clock);
}

}  // namespace cronet
