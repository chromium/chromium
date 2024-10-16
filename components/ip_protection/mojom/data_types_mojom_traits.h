// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_MOJOM_DATA_TYPES_MOJOM_TRAITS_H_
#define COMPONENTS_IP_PROTECTION_MOJOM_DATA_TYPES_MOJOM_TRAITS_H_

#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/mojom/data_types.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

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

}  // namespace mojo

#endif  // COMPONENTS_IP_PROTECTION_MOJOM_DATA_TYPES_MOJOM_TRAITS_H_
