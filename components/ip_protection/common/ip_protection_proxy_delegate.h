// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_DELEGATE_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_DELEGATE_H_

#include <deque>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "components/ip_protection/common/ip_protection_core.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/masked_domain_list_manager.h"
#include "net/base/proxy_delegate.h"
#include "net/proxy_resolution/proxy_retry_info.h"

namespace net {
class HttpRequestHeaders;
class ProxyResolutionService;
}  // namespace net

namespace ip_protection {

// IpProtectionProxyDelegate is used to support IP protection, by injecting
// proxies for requests where IP should be protected.
class IpProtectionProxyDelegate : public net::ProxyDelegate {
 public:
  // Both masked_domain_list_manager and ipp_core must be
  // non-null. The masked_domain_list_manager (MaskedDomainList) feature
  // must be enabled.
  IpProtectionProxyDelegate(MaskedDomainListManager* masked_domain_list_manager,
                            std::unique_ptr<IpProtectionCore> ipp_core);

  IpProtectionProxyDelegate(const IpProtectionProxyDelegate&) = delete;
  IpProtectionProxyDelegate& operator=(const IpProtectionProxyDelegate&) =
      delete;

  ~IpProtectionProxyDelegate() override;

  // net::ProxyDelegate implementation:
  void OnResolveProxy(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const std::string& method,
      const net::ProxyRetryInfoMap& proxy_retry_info,
      net::ProxyInfo* result) override;
  void OnSuccessfulRequestAfterFailures(
      const net::ProxyRetryInfoMap& proxy_retry_info) override;
  void OnFallback(const net::ProxyChain& bad_chain, int net_error) override;
  net::Error OnBeforeTunnelRequest(
      const net::ProxyChain& proxy_chain,
      size_t chain_index,
      net::HttpRequestHeaders* extra_headers) override;
  net::Error OnTunnelHeadersReceived(
      const net::ProxyChain& proxy_chain,
      size_t chain_index,
      const net::HttpResponseHeaders& response_headers) override;
  void SetProxyResolutionService(
      net::ProxyResolutionService* proxy_resolution_service) override;

 private:
  friend class IpProtectionProxyDelegateTest;
  FRIEND_TEST_ALL_PREFIXES(IpProtectionProxyDelegateTest, MergeProxyRules);

  ProxyResolutionResult ClassifyRequest(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      net::ProxyInfo* result);

  // Returns the equivalent of replacing all DIRECT proxies in
  // `existing_proxy_list` with the proxies in `custom_proxy_list`.
  static net::ProxyList MergeProxyRules(
      const net::ProxyList& existing_proxy_list,
      const net::ProxyList& custom_proxy_list);

  const raw_ptr<MaskedDomainListManager> masked_domain_list_manager_;

  const std::unique_ptr<IpProtectionCore> ipp_core_;

  base::WeakPtrFactory<IpProtectionProxyDelegate> weak_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_DELEGATE_H_
