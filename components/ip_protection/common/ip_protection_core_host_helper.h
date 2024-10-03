// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_HOST_HELPER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_HOST_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/get_proxy_config.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace quiche {
struct BlindSignToken;
}  // namespace quiche

namespace ip_protection {

// Contains static variables and methods for IpProtectionCoreHosts.
//
// It is the implementation's job to actually get the IP protection tokens on
// demand for the network service. This interface defines methods and variables
// commonly used for config providers that implement it. Derived classes must
// contain instances of `IpProtectionProxyConfigRetriever`,
// `quiche::BlindSignAuth`, and some implementation of
// `quiche::BlindSignMessageInterface`.
class IpProtectionCoreHostHelper {
 public:
  virtual ~IpProtectionCoreHostHelper() = default;

  // Creates a blind-signed auth token by converting token fetched using the
  // `quiche::BlindSignAuth` library to a `BlindSignedAuthToken`.
  static std::optional<BlindSignedAuthToken> CreateBlindSignedAuthToken(
      const quiche::BlindSignToken& bsa_token);

  // Creates a `quiche::BlindSignToken()` in the format that the BSA library
  // will return them.
  static quiche::BlindSignToken CreateBlindSignTokenForTesting(
      std::string token_value,
      base::Time expiration,
      const GeoHint& geo_hint);

  static privacy::ppn::PrivacyPassTokenData CreatePrivacyPassTokenForTesting(
      std::string token_value);

  // Converts a mock token value and expiration time into the struct that will
  // be passed to the network service.
  static std::optional<BlindSignedAuthToken>
  CreateMockBlindSignedAuthTokenForTesting(std::string token_value,
                                           base::Time expiration,
                                           const GeoHint& geo_hint);

  // Service types used for GetProxyConfigRequest.
  static constexpr char kChromeIpBlinding[] = "chromeipblinding";
  static constexpr char kWebViewIpBlinding[] = "webviewipblinding";

  // Base time deltas for calculating `try_again_after`.
  static constexpr base::TimeDelta kNotEligibleBackoff = base::Days(1);
  static constexpr base::TimeDelta kTransientBackoff = base::Seconds(5);
  static constexpr base::TimeDelta kBugBackoff = base::Minutes(10);
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_HOST_HELPER_H_
