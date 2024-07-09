// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/ip_protection_config_provider_helper.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/blind_sign_auth_options.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/time/time.h"

// static
std::vector<net::ProxyChain>
IpProtectionConfigProviderHelper::GetProxyListFromProxyConfigResponse(
    ip_protection::GetProxyConfigResponse response) {
  // Shortcut to create a ProxyServer with SCHEME_HTTPS from a string in the
  // proto.
  auto add_server = [](std::vector<net::ProxyServer>& proxies,
                       std::string host) {
    net::ProxyServer proxy_server = net::ProxySchemeHostAndPortToProxyServer(
        net::ProxyServer::SCHEME_HTTPS, host);
    if (!proxy_server.is_valid()) {
      return false;
    }
    proxies.push_back(proxy_server);
    return true;
  };

  std::vector<net::ProxyChain> proxy_list;
  for (const auto& proxy_chain : response.proxy_chain()) {
    std::vector<net::ProxyServer> proxies;
    bool ok = true;
    bool overridden = false;
    if (const std::string a_override =
            net::features::kIpPrivacyProxyAHostnameOverride.Get();
        a_override != "") {
      overridden = true;
      ok = ok && add_server(proxies, a_override);
    } else {
      ok = ok && add_server(proxies, proxy_chain.proxy_a());
    }
    if (const std::string b_override =
            net::features::kIpPrivacyProxyBHostnameOverride.Get();
        ok && b_override != "") {
      overridden = true;
      ok = ok && add_server(proxies, b_override);
    } else {
      ok = ok && add_server(proxies, proxy_chain.proxy_b());
    }

    // Create a new ProxyChain if the proxies were all valid.
    if (ok) {
      // If the `chain_id` is out of range or local features overrode the
      // chain, use the proxy chain anyway, but with the default `chain_id`.
      // This allows adding new IDs on the server side without breaking older
      // browsers.
      int chain_id = proxy_chain.chain_id();
      if (overridden || chain_id < 0 ||
          chain_id > net::ProxyChain::kMaxIpProtectionChainId) {
        chain_id = net::ProxyChain::kDefaultIpProtectionChainId;
      }
      proxy_list.push_back(
          net::ProxyChain::ForIpProtection(std::move(proxies), chain_id));
    }
  }

  VLOG(2) << "IPATP::GetProxyList got proxy list of length "
          << proxy_list.size();

  return proxy_list;
}

// static
network::mojom::GeoHintPtr
IpProtectionConfigProviderHelper::GetGeoHintFromProxyConfigResponse(
    const ip_protection::GetProxyConfigResponse& response) {
  if (!response.has_geo_hint()) {
    return nullptr;  // No GeoHint available in the response.
  }

  const auto& response_geo_hint = response.geo_hint();

  return network::mojom::GeoHint::New(response_geo_hint.country_code(),
                                      response_geo_hint.iso_region(),
                                      response_geo_hint.city_name());
}

// static
network::mojom::BlindSignedAuthTokenPtr
IpProtectionConfigProviderHelper::CreateBlindSignedAuthToken(
    const quiche::BlindSignToken& bsa_token) {
  // If a GeoHint's country code is empty, the token is invalid. Return a
  // nullptr.
  if (bsa_token.geo_hint.country_code.empty()) {
    return nullptr;
  }

  // Set expiration of BlindSignedAuthToken.
  base::Time expiration =
      base::Time::FromTimeT(absl::ToTimeT(bsa_token.expiration));

  // Set token of BlindSignedAuth Token to be the fully constructed
  // authorization header value.
  std::string token_header_value = "";
  privacy::ppn::PrivacyPassTokenData privacy_pass_token_data;
  if (privacy_pass_token_data.ParseFromString(bsa_token.token)) {
    token_header_value =
        base::StrCat({"PrivateToken token=\"", privacy_pass_token_data.token(),
                      "\", extensions=\"",
                      privacy_pass_token_data.encoded_extensions(), "\""});
  }

  // Set GeoHint on BlindSignedAuthToken.
  auto geo_hint = network::mojom::GeoHint::New(bsa_token.geo_hint.country_code,
                                               bsa_token.geo_hint.region,
                                               bsa_token.geo_hint.city);

  return network::mojom::BlindSignedAuthToken::New(
      std::move(token_header_value), expiration, std::move(geo_hint));
}

// static
privacy::ppn::PrivacyPassTokenData
IpProtectionConfigProviderHelper::CreatePrivacyPassTokenForTesting(
    std::string token_value) {
  privacy::ppn::PrivacyPassTokenData privacy_pass_token_data;

  // The PrivacyPassTokenData values get base64-encoded by BSA, so simulate that
  // here.
  std::string encoded_token_value = base::Base64Encode(token_value);
  std::string encoded_extension_value =
      base::Base64Encode("mock-extension-value");

  privacy_pass_token_data.set_token(std::move(encoded_token_value));
  privacy_pass_token_data.set_encoded_extensions(
      std::move(encoded_extension_value));

  return privacy_pass_token_data;
}

quiche::BlindSignToken
IpProtectionConfigProviderHelper::CreateBlindSignTokenForTesting(
    std::string token_value,
    base::Time expiration,
    const network::mojom::GeoHint& geo_hint) {
  privacy::ppn::PrivacyPassTokenData privacy_pass_token_data =
      CreatePrivacyPassTokenForTesting(std::move(token_value));
  quiche::BlindSignToken blind_sign_token;
  blind_sign_token.token = privacy_pass_token_data.SerializeAsString();
  blind_sign_token.expiration = absl::FromTimeT(expiration.ToTimeT());

  // GeoHints must contain a country level coarseness. If finer levels of
  // granularity are omitted, the GeoHint.geo_hint will contain trailing commas.
  blind_sign_token.geo_hint = anonymous_tokens::GeoHint{
      .geo_hint = geo_hint.country_code + "," + geo_hint.iso_region + "," +
                  geo_hint.city_name,
      .country_code = geo_hint.country_code,
      .region = geo_hint.iso_region,
      .city = geo_hint.city_name,
  };

  return blind_sign_token;
}

net::ProxyChain IpProtectionConfigProviderHelper::MakeChainForTesting(
    std::vector<std::string> hostnames,
    int chain_id) {
  std::vector<net::ProxyServer> servers;
  for (auto& hostname : hostnames) {
    servers.push_back(net::ProxyServer::FromSchemeHostAndPort(
        net::ProxyServer::SCHEME_HTTPS, hostname, std::nullopt));
  }
  return net::ProxyChain::ForIpProtection(servers, chain_id);
}

network::mojom::BlindSignedAuthTokenPtr
IpProtectionConfigProviderHelper::CreateMockBlindSignedAuthTokenForTesting(
    std::string token_value,
    base::Time expiration,
    const network::mojom::GeoHint& geo_hint) {
  quiche::BlindSignToken blind_sign_token =
      CreateBlindSignTokenForTesting(token_value, expiration, geo_hint);
  return CreateBlindSignedAuthToken(std::move(blind_sign_token));
}
