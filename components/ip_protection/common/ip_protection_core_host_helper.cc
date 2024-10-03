// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_core_host_helper.h"

#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "net/base/features.h"
#include "net/base/proxy_string_util.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/blind_sign_auth_options.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/time/time.h"

namespace ip_protection {

// static
std::optional<BlindSignedAuthToken>
IpProtectionCoreHostHelper::CreateBlindSignedAuthToken(
    const quiche::BlindSignToken& bsa_token) {
  // If a GeoHint's country code is empty, the token is invalid. Return a
  // nullptr.
  if (bsa_token.geo_hint.country_code.empty()) {
    return std::nullopt;
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
  GeoHint geo_hint = {
      .country_code = bsa_token.geo_hint.country_code,
      .iso_region = bsa_token.geo_hint.region,
      .city_name = bsa_token.geo_hint.city,
  };

  return std::make_optional<BlindSignedAuthToken>(
      {.token = std::move(token_header_value),
       .expiration = expiration,
       .geo_hint = std::move(geo_hint)});
}

// static
privacy::ppn::PrivacyPassTokenData
IpProtectionCoreHostHelper::CreatePrivacyPassTokenForTesting(
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
IpProtectionCoreHostHelper::CreateBlindSignTokenForTesting(
    std::string token_value,
    base::Time expiration,
    const GeoHint& geo_hint) {
  privacy::ppn::PrivacyPassTokenData privacy_pass_token_data =
      CreatePrivacyPassTokenForTesting(std::move(token_value));  // IN-TEST
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

std::optional<BlindSignedAuthToken>
IpProtectionCoreHostHelper::CreateMockBlindSignedAuthTokenForTesting(
    std::string token_value,
    base::Time expiration,
    const GeoHint& geo_hint) {
  quiche::BlindSignToken blind_sign_token = CreateBlindSignTokenForTesting(
      token_value, expiration, geo_hint);  // IN-TEST
  return CreateBlindSignedAuthToken(std::move(blind_sign_token));
}

}  // namespace ip_protection
