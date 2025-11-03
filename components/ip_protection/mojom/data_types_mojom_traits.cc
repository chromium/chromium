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

// static
ip_protection::mojom::IpProxyStatus EnumTraits<
    ip_protection::mojom::IpProxyStatus,
    ip_protection::IpProxyStatus>::ToMojom(ip_protection::IpProxyStatus input) {
  switch (input) {
    case ip_protection::IpProxyStatus::kOk:
      return ip_protection::mojom::IpProxyStatus::kOk;
    case ip_protection::IpProxyStatus::kFeatureNotEnabled:
      return ip_protection::mojom::IpProxyStatus::kFeatureNotEnabled;
    case ip_protection::IpProxyStatus::kMaskedDomainListNotEnabled:
      return ip_protection::mojom::IpProxyStatus::kMaskedDomainListNotEnabled;
    case ip_protection::IpProxyStatus::kMaskedDomainListNotPopulated:
      return ip_protection::mojom::IpProxyStatus::kMaskedDomainListNotPopulated;
    case ip_protection::IpProxyStatus::kAuthTokensUnavailable:
      return ip_protection::mojom::IpProxyStatus::kAuthTokensUnavailable;
    case ip_protection::IpProxyStatus::kUnavailable:
      return ip_protection::mojom::IpProxyStatus::kUnavailable;
    case ip_protection::IpProxyStatus::kBypassedByDevTools:
      return ip_protection::mojom::IpProxyStatus::kBypassedByDevTools;
  }
  NOTREACHED();
}

// static
bool EnumTraits<ip_protection::mojom::IpProxyStatus,
                ip_protection::IpProxyStatus>::
    FromMojom(ip_protection::mojom::IpProxyStatus input,
              ip_protection::IpProxyStatus* output) {
  switch (input) {
    case ip_protection::mojom::IpProxyStatus::kOk:
      *output = ip_protection::IpProxyStatus::kOk;
      return true;
    case ip_protection::mojom::IpProxyStatus::kFeatureNotEnabled:
      *output = ip_protection::IpProxyStatus::kFeatureNotEnabled;
      return true;
    case ip_protection::mojom::IpProxyStatus::kMaskedDomainListNotEnabled:
      *output = ip_protection::IpProxyStatus::kMaskedDomainListNotEnabled;
      return true;
    case ip_protection::mojom::IpProxyStatus::kMaskedDomainListNotPopulated:
      *output = ip_protection::IpProxyStatus::kMaskedDomainListNotPopulated;
      return true;
    case ip_protection::mojom::IpProxyStatus::kAuthTokensUnavailable:
      *output = ip_protection::IpProxyStatus::kAuthTokensUnavailable;
      return true;
    case ip_protection::mojom::IpProxyStatus::kUnavailable:
      *output = ip_protection::IpProxyStatus::kUnavailable;
      return true;
    case ip_protection::mojom::IpProxyStatus::kBypassedByDevTools:
      *output = ip_protection::IpProxyStatus::kBypassedByDevTools;
      return true;
  }
  return false;
}

}  // namespace mojo
