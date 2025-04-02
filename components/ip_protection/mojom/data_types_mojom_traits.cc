// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/mojom/data_types_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

// static
ip_protection::mojom::ProxyLayer EnumTraits<
    ip_protection::mojom::ProxyLayer,
    ip_protection::ProxyLayer>::ToMojom(ip_protection::ProxyLayer input) {
  switch (input) {
    case ip_protection::ProxyLayer::kProxyA:
      return ip_protection::mojom::ProxyLayer::kProxyA;
    case ip_protection::ProxyLayer::kProxyB:
      return ip_protection::mojom::ProxyLayer::kProxyB;
  }

  // Failure to convert should never occur.
  NOTREACHED();
}

// static
bool EnumTraits<ip_protection::mojom::ProxyLayer, ip_protection::ProxyLayer>::
    FromMojom(ip_protection::mojom::ProxyLayer input,
              ip_protection::ProxyLayer* output) {
  switch (input) {
    case ip_protection::mojom::ProxyLayer::kProxyA:
      *output = ip_protection::ProxyLayer::kProxyA;
      return true;
    case ip_protection::mojom::ProxyLayer::kProxyB:
      *output = ip_protection::ProxyLayer::kProxyB;
      return true;
  }

  // Return `false` to indicate the conversion was not successful.
  return false;
}

bool StructTraits<
    ip_protection::mojom::GeoHintDataView,
    ip_protection::GeoHint>::Read(ip_protection::mojom::GeoHintDataView data,
                                  ip_protection::GeoHint* out) {
  return data.ReadCountryCode(&out->country_code) &&
         data.ReadIsoRegion(&out->iso_region) &&
         data.ReadCityName(&out->city_name);
}

bool StructTraits<ip_protection::mojom::BlindSignedAuthTokenDataView,
                  ip_protection::BlindSignedAuthToken>::
    Read(ip_protection::mojom::BlindSignedAuthTokenDataView data,
         ip_protection::BlindSignedAuthToken* out) {
  return data.ReadToken(&out->token) && data.ReadExpiration(&out->expiration) &&
         data.ReadGeoHint(&out->geo_hint);
}

ip_protection::mojom::TryGetProbabilisticRevealTokensStatus
EnumTraits<ip_protection::mojom::TryGetProbabilisticRevealTokensStatus,
           ip_protection::TryGetProbabilisticRevealTokensStatus>::
    ToMojom(ip_protection::TryGetProbabilisticRevealTokensStatus input) {
  switch (input) {
    case ip_protection::TryGetProbabilisticRevealTokensStatus::kSuccess:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kSuccess;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::kNetNotOk:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kNetNotOk;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::
        kNetOkNullResponse:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kNetOkNullResponse;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::kNullResponse:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kNullResponse;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::
        kResponseParsingFailed:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kResponseParsingFailed;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::
        kInvalidTokenVersion:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kInvalidTokenVersion;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::
        kInvalidTokenSize:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kInvalidTokenSize;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::kTooFewTokens:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kTooFewTokens;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::kTooManyTokens:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kTooManyTokens;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::
        kExpirationTooSoon:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kExpirationTooSoon;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::
        kExpirationTooLate:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kExpirationTooLate;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::
        kInvalidPublicKey:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kInvalidPublicKey;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::
        kInvalidNumTokensWithSignal:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kInvalidNumTokensWithSignal;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::
        kRequestBackedOff:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kRequestBackedOff;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::
        kNoGoogleChromeBranding:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kNoGoogleChromeBranding;
    case ip_protection::TryGetProbabilisticRevealTokensStatus::
        kInvalidEpochIdSize:
      return ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
          kInvalidEpochIdSize;
  }
  // Failure to convert should never occur.
  NOTREACHED();
}

bool EnumTraits<ip_protection::mojom::TryGetProbabilisticRevealTokensStatus,
                ip_protection::TryGetProbabilisticRevealTokensStatus>::
    FromMojom(ip_protection::mojom::TryGetProbabilisticRevealTokensStatus input,
              ip_protection::TryGetProbabilisticRevealTokensStatus* output) {
  switch (input) {
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::kSuccess:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::kSuccess;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::kNetNotOk:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::kNetNotOk;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kNetOkNullResponse:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::
          kNetOkNullResponse;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kNullResponse:
      *output =
          ip_protection::TryGetProbabilisticRevealTokensStatus::kNullResponse;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kResponseParsingFailed:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::
          kResponseParsingFailed;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kInvalidTokenVersion:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::
          kInvalidTokenVersion;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kInvalidTokenSize:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::
          kInvalidTokenSize;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kTooFewTokens:
      *output =
          ip_protection::TryGetProbabilisticRevealTokensStatus::kTooFewTokens;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kTooManyTokens:
      *output =
          ip_protection::TryGetProbabilisticRevealTokensStatus::kTooManyTokens;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kExpirationTooSoon:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::
          kExpirationTooSoon;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kExpirationTooLate:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::
          kExpirationTooLate;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kInvalidPublicKey:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::
          kInvalidPublicKey;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kInvalidNumTokensWithSignal:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::
          kInvalidNumTokensWithSignal;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kRequestBackedOff:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::
          kRequestBackedOff;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kNoGoogleChromeBranding:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::
          kNoGoogleChromeBranding;
      return true;
    case ip_protection::mojom::TryGetProbabilisticRevealTokensStatus::
        kInvalidEpochIdSize:
      *output = ip_protection::TryGetProbabilisticRevealTokensStatus::
          kInvalidEpochIdSize;
      return true;
  }
  return false;
}

bool StructTraits<
    ip_protection::mojom::TryGetProbabilisticRevealTokensResultDataView,
    ip_protection::TryGetProbabilisticRevealTokensResult>::
    Read(ip_protection::mojom::TryGetProbabilisticRevealTokensResultDataView
             data,
         ip_protection::TryGetProbabilisticRevealTokensResult* out) {
  out->network_error_code = data.network_error_code();
  return data.ReadStatus(&out->status) &&
         data.ReadTryAgainAfter(&out->try_again_after);
}

bool StructTraits<ip_protection::mojom::ProbabilisticRevealTokenDataView,
                  ip_protection::ProbabilisticRevealToken>::
    Read(ip_protection::mojom::ProbabilisticRevealTokenDataView data,
         ip_protection::ProbabilisticRevealToken* out) {
  out->version = data.version();
  return data.ReadU(&out->u) && data.ReadE(&out->e);
}

bool StructTraits<
    ip_protection::mojom::TryGetProbabilisticRevealTokensOutcomeDataView,
    ip_protection::TryGetProbabilisticRevealTokensOutcome>::
    Read(ip_protection::mojom::TryGetProbabilisticRevealTokensOutcomeDataView
             data,
         ip_protection::TryGetProbabilisticRevealTokensOutcome* out) {
  out->expiration_time_seconds = data.expiration_time_seconds();
  out->next_epoch_start_time_seconds = data.next_epoch_start_time_seconds();
  out->num_tokens_with_signal = data.num_tokens_with_signal();
  return data.ReadTokens(&out->tokens) &&
         data.ReadPublicKey(&out->public_key) &&
         data.ReadEpochId(&out->epoch_id);
}

}  // namespace mojo
