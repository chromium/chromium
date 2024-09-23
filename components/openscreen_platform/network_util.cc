// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/openscreen_platform/network_util.h"

#include <array>
#include <memory>

#include "base/notreached.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"

namespace openscreen_platform {

const net::IPAddress ToNetAddress(const openscreen::IPAddress& address) {
  switch (address.version()) {
    case openscreen::IPAddress::Version::kV4: {
      std::array<uint8_t, openscreen::IPAddress::kV4Size> bytes_v4;
      address.CopyToV4(bytes_v4.data());
      return net::IPAddress(bytes_v4);
    }
    case openscreen::IPAddress::Version::kV6: {
      std::array<uint8_t, openscreen::IPAddress::kV6Size> bytes_v6;
      address.CopyToV6(bytes_v6.data());
      return net::IPAddress(bytes_v6);
    }
  }
}

const net::IPEndPoint ToNetEndPoint(const openscreen::IPEndpoint& endpoint) {
  return net::IPEndPoint(ToNetAddress(endpoint.address), endpoint.port);
}

openscreen::IPAddress::Version ToOpenScreenVersion(
    const net::AddressFamily family) {
  switch (family) {
    case net::AddressFamily::ADDRESS_FAMILY_IPV6:
      return openscreen::IPAddress::Version::kV6;
    case net::AddressFamily::ADDRESS_FAMILY_IPV4:
      return openscreen::IPAddress::Version::kV4;

    case net::AddressFamily::ADDRESS_FAMILY_UNSPECIFIED:
      NOTREACHED_IN_MIGRATION();
      return openscreen::IPAddress::Version::kV4;
  }
}

const openscreen::IPEndpoint ToOpenScreenEndPoint(
    const net::IPEndPoint& endpoint) {
  const openscreen::IPAddress::Version version =
      ToOpenScreenVersion(endpoint.GetFamily());
  return openscreen::IPEndpoint{
      openscreen::IPAddress{version, endpoint.address().bytes().data()},
      endpoint.port()};
}

}  // namespace openscreen_platform
