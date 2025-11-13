// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/content_settings/core/common/content_settings.h"
class GURL;

namespace net {

class ProxyChain;
class NetworkAnonymizationKey;

}  // namespace net

namespace ip_protection {

struct BlindSignedAuthToken;
enum class IpProxyStatus;
// Core business logic for IP Protection.
class IpProtectionCore {
 public:
  virtual ~IpProtectionCore() = default;

  virtual bool IsIpProtectionEnabled() = 0;

  // Check whether the masked domain list is populated.
  virtual bool IsMdlPopulated() = 0;

  // Check whether the given request URL and NAK should be proxied.
  virtual bool RequestShouldBeProxied(
      const GURL& request_url,
      const net::NetworkAnonymizationKey& network_anonymization_key) = 0;

  // Check whether tokens are available in all token caches.
  //
  // This function is called on every URL load, so it should complete quickly.
  virtual bool AreAuthTokensAvailable() = 0;

  // Check whether the tokens in either cache have ever been filled.
  //
  // If even one cache has not been filled at least once, this method should
  // return false. Also, this method will ALWAYS return false if the proxy list
  // is unavailable.
  virtual bool WereTokenCachesEverFilled() = 0;

  // Get a token, if one is available.
  //
  // Returns `nullopt` if no token is available, whether for a transient or
  // permanent reason. This method may return `nullopt` even if
  // `IsAuthTokenAvailable()` recently returned `true`.
  virtual std::optional<BlindSignedAuthToken> GetAuthToken(
      size_t chain_index) = 0;

  // Check whether a proxy chain list is available.
  virtual bool IsProxyListAvailable() = 0;

  // Notifies that QUIC proxies failed for a request, suggesting that QUIC may
  // not work on this network.
  virtual void QuicProxiesFailed() = 0;

  // Return the currently cached proxy chain lists. This contains the lists of
  // hostnames corresponding to each proxy chain that should be used. This
  // may be empty even if `IsProxyListAvailable()` returned true.
  virtual std::vector<net::ProxyChain> GetProxyChainList() = 0;

  // Request a refresh of the proxy chain list. Call this when it's likely that
  // the proxy chain list is out of date.
  virtual void RequestRefreshProxyList() = 0;

  // Callback function used by `IpProtectionProxyConfigManager` and
  // `IpProtectionTokenManager` to signal a possible geo change due to a
  // refreshed proxy list or refill of tokens.
  virtual void GeoObserved(const std::string& geo_id) = 0;

  // Returns whether `first_party_url` has a tracking protection exception. This
  // url can match a subdomain of an exception (i.e. an exception for
  // example.com covers all domains in this pattern: [*.]example.com).
  virtual bool HasTrackingProtectionException(
      const GURL& first_party_url) const = 0;

  // Sets the TRACKING_PROTECTION content settings list to `settings`.
  virtual void SetTrackingProtectionContentSetting(
      const ContentSettingsForOneType& settings) = 0;

  // Signal that a token was demanded by the proxy delegate, for metrics.
  virtual void RecordTokenDemand(size_t chain_index) = 0;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_H_
