// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/metrics_util.h"

#include "base/logging.h"

namespace chromecast {

uint32_t GetIPAddressFragmentForLogging(const net::IPAddressBytes& sender_ip) {
  // Check if address is valid IPv4 or IPv6 byte array. If not then fill in
  // with zeros as a default value.
  if (sender_ip.size() != net::IPAddress::kIPv4AddressSize &&
      sender_ip.size() != net::IPAddress::kIPv6AddressSize) {
    DVLOG(1) << "Sender IP is not IPv4 or IPv6; zeroing out sender fragment.";
    return 0;
  }
  // Grab the last 2 bytes of sender IP address in network order and store as
  // packed 16-bit integer. The unused bits in the final packed value should
  // be empty.
  uint32_t packed_address = 0;
  if (sender_ip.size() >= 2) {
    int i = sender_ip.size() - 1;
    packed_address |= (sender_ip[i--]);
    packed_address |= (sender_ip[i--] << 8);
  }

  return packed_address;
}

}  // namespace chromecast
