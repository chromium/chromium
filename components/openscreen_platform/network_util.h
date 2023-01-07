// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_NETWORK_UTIL_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_NETWORK_UTIL_H_

#include "net/base/address_family.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"

namespace net {
class IPAddress;
class IPEndPoint;
}  // namespace net

// Helper methods that convert between Open Screen and Chromium //net types.
namespace openscreen_platform {

const net::IPAddress ToNetAddress(const openscreen::IPAddress& address);
const net::IPEndPoint ToNetEndPoint(const openscreen::IPEndpoint& endpoint);
openscreen::IPAddress::Version ToOpenScreenVersion(
    const net::AddressFamily family);
const openscreen::IPEndpoint ToOpenScreenEndPoint(
    const net::IPEndPoint& endpoint);

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_NETWORK_UTIL_H_
