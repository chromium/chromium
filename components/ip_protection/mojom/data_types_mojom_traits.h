// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_MOJOM_DATA_TYPES_MOJOM_TRAITS_H_
#define COMPONENTS_IP_PROTECTION_MOJOM_DATA_TYPES_MOJOM_TRAITS_H_

#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/mojom/data_types.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

// Converts ip_protection::mojom::ProxyLayer to/from ip_protection::ProxyLayer,
// so that ip_protection::ProxyLayer can be used throughout the codebase without
// any direct reference to ip_protection::mojom::ProxyLayer.
template <>
struct EnumTraits<ip_protection::mojom::ProxyLayer, ip_protection::ProxyLayer> {
  static ip_protection::mojom::ProxyLayer ToMojom(ip_protection::ProxyLayer);
  static bool FromMojom(ip_protection::mojom::ProxyLayer input,
                        ip_protection::ProxyLayer* output);
};

// Converts ip_protection::mojom::GeoHint to/from ip_protection::GeoHint,
// so that ip_protection::GeoHint can be used throughout the codebase without
// any direct reference to ip_protection::mojom::GeoHint.
template <>
struct StructTraits<ip_protection::mojom::GeoHintDataView,
                    ip_protection::GeoHint> {
  static const std::string& country_code(const ip_protection::GeoHint& r) {
    return r.country_code;
  }
  static const std::string& iso_region(const ip_protection::GeoHint& r) {
    return r.iso_region;
  }
  static const std::string& city_name(const ip_protection::GeoHint& r) {
    return r.city_name;
  }

  // If Read() returns false, Mojo will discard the message.
  static bool Read(ip_protection::mojom::GeoHintDataView data,
                   ip_protection::GeoHint* out);
};

// Converts ip_protection::mojom::BlindSignedAuthToken to/from
// BlindSignedAuthToken, so that BlindSignedAuthToken can be used throughout the
// codebase without any direct reference to
// ip_protection::mojom::BlindSignedAuthToken.
template <>
struct StructTraits<ip_protection::mojom::BlindSignedAuthTokenDataView,
                    ip_protection::BlindSignedAuthToken> {
  static const std::string& token(
      const ip_protection::BlindSignedAuthToken& r) {
    return r.token;
  }
  static const base::Time expiration(
      const ip_protection::BlindSignedAuthToken& r) {
    return r.expiration;
  }
  static const ip_protection::GeoHint& geo_hint(
      const ip_protection::BlindSignedAuthToken& r) {
    return r.geo_hint;
  }

  // If Read() returns false, Mojo will discard the message.
  static bool Read(ip_protection::mojom::BlindSignedAuthTokenDataView data,
                   ip_protection::BlindSignedAuthToken* out);
};

// Converts ip_protection::mojom::TryGetProbabilisticRevealTokensStatus to/from
// ip_protection::TryGetProbabilisticRevealTokensStatus,
// so that ip_protection::TryGetProbabilisticRevealTokensStatus can be used
// throughout the codebase without any direct reference to
// ip_protection::mojom::TryGetProbabilisticRevealTokensStatus.
template <>
struct EnumTraits<ip_protection::mojom::TryGetProbabilisticRevealTokensStatus,
                  ip_protection::TryGetProbabilisticRevealTokensStatus> {
  static ip_protection::mojom::TryGetProbabilisticRevealTokensStatus ToMojom(
      ip_protection::TryGetProbabilisticRevealTokensStatus);
  static bool FromMojom(
      ip_protection::mojom::TryGetProbabilisticRevealTokensStatus input,
      ip_protection::TryGetProbabilisticRevealTokensStatus* output);
};

// Converts ip_protection::mojom::TryGetProbabilisticRevealTokensResult to/from
// ip_protection::TryGetProbabilisticRevealTokensResult.
template <>
struct StructTraits<
    ip_protection::mojom::TryGetProbabilisticRevealTokensResultDataView,
    ip_protection::TryGetProbabilisticRevealTokensResult> {
  static ip_protection::TryGetProbabilisticRevealTokensStatus status(
      const ip_protection::TryGetProbabilisticRevealTokensResult& r) {
    return r.status;
  }
  static int32_t network_error_code(
      const ip_protection::TryGetProbabilisticRevealTokensResult& r) {
    return r.network_error_code;
  }
  static std::optional<base::Time> try_again_after(
      const ip_protection::TryGetProbabilisticRevealTokensResult& r) {
    return r.try_again_after;
  }

  // If Read() returns false, Mojo will discard the message.
  static bool Read(
      ip_protection::mojom::TryGetProbabilisticRevealTokensResultDataView data,
      ip_protection::TryGetProbabilisticRevealTokensResult* out);
};

// Converts ip_protection::mojom::ProbabilisticRevealToken to/from
// ip_protection::ProbabilisticRevealToken.
template <>
struct StructTraits<ip_protection::mojom::ProbabilisticRevealTokenDataView,
                    ip_protection::ProbabilisticRevealToken> {
  static int32_t version(const ip_protection::ProbabilisticRevealToken& prt) {
    return prt.version;
  }

  static const std::string& u(
      const ip_protection::ProbabilisticRevealToken& prt) {
    return prt.u;
  }

  static const std::string& e(
      const ip_protection::ProbabilisticRevealToken& prt) {
    return prt.e;
  }

  // If Read() returns false, Mojo will discard the message.
  static bool Read(ip_protection::mojom::ProbabilisticRevealTokenDataView data,
                   ip_protection::ProbabilisticRevealToken* out);
};

// Converts ip_protection::mojom::TryGetProbabilisticRevealTokensOutcome to/from
// ip_protection::TryGetProbabilisticRevealTokensOutcome.
template <>
struct StructTraits<
    ip_protection::mojom::TryGetProbabilisticRevealTokensOutcomeDataView,
    ip_protection::TryGetProbabilisticRevealTokensOutcome> {
  static const std::vector<ip_protection::ProbabilisticRevealToken>& tokens(
      const ip_protection::TryGetProbabilisticRevealTokensOutcome& outcome) {
    return outcome.tokens;
  }

  static const std::string& public_key(
      const ip_protection::TryGetProbabilisticRevealTokensOutcome& outcome) {
    return outcome.public_key;
  }

  static uint64_t expiration_time_seconds(
      const ip_protection::TryGetProbabilisticRevealTokensOutcome& outcome) {
    return outcome.expiration_time_seconds;
  }

  static uint64_t next_epoch_start_time_seconds(
      const ip_protection::TryGetProbabilisticRevealTokensOutcome& outcome) {
    return outcome.next_epoch_start_time_seconds;
  }

  static int32_t num_tokens_with_signal(
      const ip_protection::TryGetProbabilisticRevealTokensOutcome& outcome) {
    return outcome.num_tokens_with_signal;
  }

  static const std::string& epoch_id(
      const ip_protection::TryGetProbabilisticRevealTokensOutcome& outcome) {
    return outcome.epoch_id;
  }

  // If Read() returns false, Mojo will discard the message.
  static bool Read(
      ip_protection::mojom::TryGetProbabilisticRevealTokensOutcomeDataView data,
      ip_protection::TryGetProbabilisticRevealTokensOutcome* out);
};

}  // namespace mojo

#endif  // COMPONENTS_IP_PROTECTION_MOJOM_DATA_TYPES_MOJOM_TRAITS_H_
