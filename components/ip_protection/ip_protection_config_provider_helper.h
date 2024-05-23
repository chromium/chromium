// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_IP_PROTECTION_CONFIG_PROVIDER_HELPER_H_
#define COMPONENTS_IP_PROTECTION_IP_PROTECTION_CONFIG_PROVIDER_HELPER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/ip_protection/get_proxy_config.pb.h"
#include "net/base/proxy_chain.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace quiche {
struct BlindSignToken;
}  // namespace quiche

// The result of a fetch of tokens from the IP Protection auth token server.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep this in sync with
// IpProtectionTokenBatchRequestResult in enums.xml.
enum class IpProtectionTryGetAuthTokensResult {
  // The request was successful and resulted in new tokens.
  kSuccess = 0,
  // No primary account is set.
  kFailedNoAccount = 1,
  // Chrome determined the primary account is not eligible.
  kFailedNotEligible = 2,
  // There was a failure fetching an OAuth token for the primary account.
  // Deprecated in favor of `kFailedOAuthToken{Transient,Persistent}`.
  kFailedOAuthTokenDeprecated = 3,
  // There was a failure in BSA with the given status code.
  kFailedBSA400 = 4,
  kFailedBSA401 = 5,
  kFailedBSA403 = 6,

  // Any other issue calling BSA.
  kFailedBSAOther = 7,

  // There was a transient failure fetching an OAuth token for the primary
  // account.
  kFailedOAuthTokenTransient = 8,
  // There was a persistent failure fetching an OAuth token for the primary
  // account.
  kFailedOAuthTokenPersistent = 9,

  // The attempt to request tokens failed because IP Protection was disabled by
  // the user.
  kFailedDisabledByUser = 10,

  kMaxValue = kFailedDisabledByUser,
};

// Contains static variables and methods for IpProtectionConfigProviders.
//
// It is the implementation's job to actually get the IP protection tokens on
// demand for the network service. This interface defines methods and variables
// commonly used for config providers that implement it. Derived classes must
// contain instances of `IpProtectionProxyConfigRetriever`,
// `quiche::BlindSignAuth`, and some implementation of
// `quiche::BlindSignMessageInterface`.
class IpProtectionConfigProviderHelper {
 public:
  virtual ~IpProtectionConfigProviderHelper() = default;

  // Creates a list of ProxyChains from GetProxyConfigResponse.
  static std::vector<net::ProxyChain> GetProxyListFromProxyConfigResponse(
      ip_protection::GetProxyConfigResponse response);

  // Creates a blind-signed auth token by converting token fetched using the
  // `quiche::BlindSignAuth` library to a
  // `network::mojom::BlindSignedAuthToken`.
  static network::mojom::BlindSignedAuthTokenPtr CreateBlindSignedAuthToken(
      const quiche::BlindSignToken& bsa_token);

  // Service types used for GetProxyConfigRequest.
  static constexpr char kChromeIpBlinding[] = "chromeipblinding";
  static constexpr char kWebViewIpBlinding[] = "webviewipblinding";

  // Base time deltas for calculating `try_again_after`.
  static constexpr base::TimeDelta kNotEligibleBackoff = base::Days(1);
  static constexpr base::TimeDelta kTransientBackoff = base::Seconds(5);
  static constexpr base::TimeDelta kBugBackoff = base::Minutes(10);
};

#endif  // COMPONENTS_IP_PROTECTION_IP_PROTECTION_CONFIG_PROVIDER_HELPER_H_
